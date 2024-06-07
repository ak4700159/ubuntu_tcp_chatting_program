#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "queue.h"

#define MAX_USERS 10000
#define WAITING true
#define RUNNING false

pthread_t td[MAX_USERS];
pthread_t queue_td[MAX_USERS];		
pthread_mutex_t cond_mtd[MAX_USERS];
pthread_mutex_t queue_mtd;
pthread_cond_t ctd[MAX_USERS];

typedef struct Thread_Info{
	int index;
	int clint_sock;
	char user_name[MAX_BUFFER_SIZE];
	bool status;                 // CONNECTING | WAITING
	Queue* queue;
}Thread_Info;
Thread_Info infos[MAX_USERS];

void init(Thread_Info* infos);
void signal_handler(int sig);
void error_handler(char* msg, int rv);
void pthread_create_handler(int rc);
void* queue_thread(void* arg);
void* server_thread(void* arg);
void send_other_clints(char* msg, int sender);


int main(int argc, char* argv[]){
	int serv_sock;
	int rc;
	struct sockaddr_in serv_addr;
	if(argc != 2){
		fprintf(stderr, "usage : ./server <server Port>\n");
		exit(1);
	}
	init(infos);
	printf("Thread pool completed\n");

	// TCP 소켓 생성 (아직 빈껍데기)
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1)
		error_handler("socket()", serv_sock);
	printf("Server socket createed\n");

	// IPv4 주소 할당 (TCP -> IP -> Port 할당)
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	// 생성된 소켓에 주소를 바인딩
	rc=bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	error_handler("bind()", rc);

	// 최대 몇 개의 클라이언트 요청을 받을 것인지
	rc = listen(serv_sock, MAX_USERS);
	error_handler("listen()", rc);

	// 위 과정이 정상적으로 작동된다면 서버 세팅은 끝
	printf("port: %d, Server starts !!! \n", atoi(argv[1]));
	printf("Clinents waiting . . .\n");
	while(1){
		struct sockaddr_in clint_addr;
		int clint_addr_len = sizeof(clint_addr);
		int clint_sock = accept(serv_sock, (struct sockaddr*)&clint_addr, &clint_addr_len);
		if(clint_sock == -1) error_handler("accept()", clint_sock);
		
		/*accept 함수를 통해 연결 요청하는 클라이언트를 기다리고
		  할당 받은 클라이언트 파일 디스크립터를 infos 구조체 넣어서
		  해당 인덱스의 스레드를 깨운다. 깨우는 순서는 순차적.*/
		for(int i = 0; i < MAX_BUFFER_SIZE; i++){
			if(infos[i].status == WAITING){
				infos[i].clint_sock = clint_sock;
				rc = pthread_cond_signal(&ctd[i]);
				error_handler("pthread_cond_signal()", rc);
				break;
			}
		}
	}

	// 서버 할당 해제, 실행될 일이 없다.
	free(&serv_sock);
	return 0;
}

// 스레드풀 준비
void init(Thread_Info* infos){
	int rc;
	signal(SIGINT, signal_handler);
	pthread_mutex_init(&queue_mtd, NULL);
	for(int i =  0; i < MAX_USERS; i++){
		pthread_mutex_init(&cond_mtd[i], NULL);
		pthread_cond_init(&ctd[i], NULL);

		infos[i].index = i;
		infos[i].status = WAITING;
		rc = pthread_create(&td[i], NULL, server_thread, (void*)&infos[i]);
		pthread_create_handler(rc);
	}
}

/*서버에서 SIGINT 발생 시 클라이언트 또한 모두 이 사실을 알고
  종료되어야 한다. 간단한 로직을 통해 동작 중인 클라이어트들에게
  종료 메시지를 전송한다.*/
void signal_handler(int sig){
	char exit_msg[] = "q";
	if(sig == SIGINT){
		for(int i = 0; i < MAX_USERS; i++){
			if(infos[i].status == RUNNING)
				write(infos[i].clint_sock, exit_msg, strlen(exit_msg));
		}
	}	
	exit(1);
}

// rv = 0 이 아니면 에러로 간주.
void error_handler(char* msg, int rv){
	if(rv){
		fprintf(stderr, "error msg : %s\n", msg);
		exit(1);
	}
}

// 큐에 담긴 메시지를 읽는 스레드
void* queue_thread(void* arg){
	Thread_Info* info = (Thread_Info*)arg;
	char* queue_msg;
	char exit_msg[] = "q";
	while(1){
		if(!isEmpty(info->queue)){
			/*
			race condition이 발생되는 구간
			왜냐하면 큐에 메시지를 어떤 클라이언트가
			push 중일 수 있기 때문.
			*/
			pthread_mutex_lock(&queue_mtd);
			queue_msg = pop(info->queue);
			pthread_mutex_unlock(&queue_mtd);
			// 클라이언트 스레드가 종료될 때 종료 메시지가 들어온다.
			// 큐 스레드 또한 종료...
			if(strncmp(queue_msg, exit_msg, 1) == 0) break;
			// 큐에 쌓인 메시지를 클라리언트로 전송
			write(info->clint_sock, queue_msg, strlen(queue_msg));
		}
	}
}

void* server_thread(void* arg){
	int rc;
	char exit_msg[MAX_BUFFER_SIZE] = "q";
	// 입장 메시지 ==== user_name님이 입장하였습니다. =====
	char entrance_msg[MAX_BUFFER_SIZE] = "";
	// 퇴장 메시지 ==== user_name님이 퇴장하였습니다. =====

	char escape_msg[MAX_BUFFER_SIZE] = ""; // 종료 메시지 q가 들어가게 됨
	char clint_msg[MAX_BUFFER_SIZE] = "";  // 클라이언트에게 받은 메시지
	
	// 클라이언트 정보가 담긴 구조체
	Thread_Info* info = (Thread_Info*)arg;

	while(1){
		pthread_mutex_lock(&cond_mtd[info->index]);
		pthread_cond_wait(&ctd[info->index], &cond_mtd[info->index]);

		// 계속해서 반복되는 과정 속에 메모리 초기화를 적절하게 잘 이뤄줘야 된다.
		memset(info->user_name, 0, sizeof(info->user_name));
		memset(entrance_msg, 0, sizeof(entrance_msg));
		memset(escape_msg, 0, sizeof(escape_msg));
		memset(clint_msg, 0, sizeof(clint_msg));
		strcat(entrance_msg, "===== ");
		strcat(escape_msg, "===== ");

		// 큐를 동적할당시키고 큐 스레드를 생성한다.
		info->queue = (Queue*)malloc(sizeof(Queue));
		//memset(info->queue, 0, sizeof(Queue));
		queue_init(info->queue);
		rc = pthread_create(&queue_td[info->index], NULL, queue_thread, (void*)info);
		pthread_create_handler(rc);

		// 모든 준비가 끝났기에 실행 상태로 바꾼다.
		info->status = RUNNING;
		// tcp 클라이언트에서 첫번째로 수신받은 메시지는 이름을 유저의 이름을 나타낸다.
		read(info->clint_sock, info->user_name, MAX_BUFFER_SIZE);

		// 입장 msg를 모든 클라이언트에 보낸다.
		strcat(entrance_msg, info->user_name);
		strcat(entrance_msg, "님이 입장하였습니다. =====");
		send_other_clints(entrance_msg, info->index);
		printf("\nClient%d : %s 사용 시작\n", info->index, info->user_name);

		// 클라이언트가 보낸 메시지를 읽고
		// 실행 중인 다른 클라이언트들에게 보낸다.
		while(1){
			memset(clint_msg, 0, sizeof(clint_msg));
			rc = read(info->clint_sock, clint_msg, MAX_BUFFER_SIZE);
			if(rc == -1) error_handler("read()", rc);
			clint_msg[rc] = '\0';

			if(strncmp(clint_msg, exit_msg, 1) == 0){
				info->status = WAITING;
				break;
			}

			printf("%s\n", clint_msg);
			send_other_clints(clint_msg, info->index);
		}
		// 퇴장 msg 또한 모든 클라이언트에 보낸다.
		strcat(escape_msg, info->user_name);
		strcat(escape_msg, "님이 퇴장하였습니다. =====");
		send_other_clints(escape_msg, info->index);

		// pthread_cancel(queue_td[info->index]) 와 동일
		push(info->queue, exit_msg);
		printf("\nClient%d : %s 사용 종료\n", info->index, info->user_name);
		pthread_mutex_unlock(&cond_mtd[info->index]);
	}
}

// sender를 제외한 모든 실행 중인 클라이언트의 큐에 메시지를 psuh한다.
void send_other_clints(char* msg, int sender){
	for(int i = 0; i < MAX_USERS; i++){
		if(infos[i].status == RUNNING && infos[i].index != sender){
			/*
			race condition 상황으로 다른 클라이언트가 같은 큐에 push
			할 수 있기 때문에 mutex lock으로 이를 방지한다.
			*/
			pthread_mutex_lock(&queue_mtd);
			push(infos[i].queue, msg);
			pthread_mutex_unlock(&queue_mtd);
		}
	}
}

void pthread_create_handler(int rc){
	if(rc == EDEADLK){
		printf("DEADLOCK detected ! \n");
	}
	else if(rc == EINVAL){
		printf("pthread_join is not joinanble ! \n");
	}
	else if(rc == ESRCH){
		printf("No thread with given ID is found ! \n");
	}
	error_handler("pthread_create()", rc);
}
