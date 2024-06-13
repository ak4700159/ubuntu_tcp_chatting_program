#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 1000
// 클라이언트 프로그램 내에서 임의 시그널 발생
#define SERVER_SIGINT 100
#define CHATTING_SIGINT 101

#define WRITER_THREAD 1
#define READER_THREAD 0

char user_name[MAX_BUFFER_SIZE]; // 클라이언트 유저 이름
int serv_sock;				     // 서버 소켓
pthread_t td[2];				 // 하나는 리더로 또 다른 하나는 라이터

void init(char* port, char* ip);
void* read_thread(void* argc);
void* write_thread(void* argc);
void signal_handler(int sig);
void error_handler(char* msg, int rv); 

/*
클라이언트 프로그램은 간단하게 진행된다.
초기화 함수를 통해 tcp 통신을 위한 준비를 하고
준비가 끝나면 리더와 라이터 스레드를 생성하고
특정 종료 이벤트가 발생하면 프로그램이 종료되는 형식
*/
int main(int argc, char* argv[]){
	int rc;
	if(argc != 4){
		fprintf(stderr, "usage : ./client <server IP> <server port> <user name>\n");
		exit(1);
	}
	strcpy(user_name, argv[3]);

	init(argv[2], argv[1]);
	rc = pthread_create(&td[WRITER_THREAD], NULL, write_thread, (void*)&serv_sock);
	error_handler("pthread_create()->write thread", rc);
	rc = pthread_create(&td[READER_THREAD], NULL, read_thread, (void*)&serv_sock);
	error_handler("pthread_create()->read thread", rc);

	rc = pthread_join(td[WRITER_THREAD], NULL);
	error_handler("pthread_join()", rc);
	rc = pthread_join(td[READER_THREAD], NULL);
	error_handler("pthread_join()", rc);

	return 0;
}

void init(char* port, char* ip){
	int rc;
	char tmp[MAX_BUFFER_SIZE];
	struct sockaddr_in serv_addr;
	signal(SIGINT, signal_handler);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1) error_handler("socket", serv_sock);
	
	// 서보 주소 할당 과정
	// TCP -> IP -> PORT
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(atoi(port));
	if(rc = connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		error_handler("connect", rc);
	printf("conected successfully!!\n");
	printf("your name : %s\n\n", user_name);

	// 서버로 user name을 보낸다.
	write(serv_sock, user_name, strlen(user_name));
	memset(tmp, 0, sizeof(tmp));
	strcat(tmp, "[");
	strcat(tmp, user_name);
	strcat(tmp, "]");
	strcpy(user_name, tmp);

}

// 다른 클라이언트 유저들이 보낸 메시지를 읽는 역할
void* read_thread(void* arg){
	int rc;
	char msg[MAX_BUFFER_SIZE];
	char exit_msg[] = "q"; 

	while(1){
		memset(msg, 0, sizeof(msg));
		rc = read(serv_sock, msg, sizeof(msg));
		if(rc < 0) error_handler("read()", rc);
		msg[rc] = '\0';
		if(strncmp(msg, exit_msg, 1) == 0) signal_handler(SERVER_SIGINT);
		printf("%s\n", msg);
	}
}

// 서버로 입력한 메시지를 전달하는 역할
void* write_thread(void* arg){
	int rc;
	char msg[MAX_BUFFER_SIZE];
	char tmp[MAX_BUFFER_SIZE];
	char exit_msg[] = "q";

	while(1){
		memset(msg, 0, MAX_BUFFER_SIZE);
		fgets(msg, MAX_BUFFER_SIZE, stdin);
		msg[strlen(msg) - 1] = '\0';
		// 편의를 위해 q만 전송해도 종료로 인식
		if(strncmp(msg, exit_msg, 1) == 0){
			while(1){
				printf("정말 종료하시겠습니까?(Y\\N)\n");				
				fgets(msg, MAX_BUFFER_SIZE, stdin);
				msg[strlen(msg) - 1] = '\0';

				if(strcmp(msg, "Y") == 0) signal_handler(CHATTING_SIGINT);
				else if(strcmp(msg, "N") == 0) break;
			}
			continue;
		}

		strcpy(tmp, user_name);
		strcat(tmp, " : ");
		strcat(tmp, msg);
		rc = write(serv_sock, tmp, strlen(tmp));
		if(rc < 0) error_handler("write()", rc);
		tmp[0] = '\0';
	}
}

/*
이 프로그램에서 다루는 시그널은 총 세 가지다.
1. SIGINT 		   : 사용자 ^C 키를 누른 경우
2. SERVER_SIGINT   : 서버에서 ^C키를 누른 경우
3. CHATTING_SIGINT : 채팅에 q메시지를 보낸 경우
*/
void signal_handler(int sig){
	char msg[MAX_BUFFER_SIZE];
	char exit_msg[MAX_BUFFER_SIZE];
	int rc;
	strcpy(exit_msg, "q");
	if(sig == SERVER_SIGINT || sig == CHATTING_SIGINT){
		printf(" ==== 채팅 강제 종료 ====\n");
		write(serv_sock, exit_msg, MAX_BUFFER_SIZE);
		rc = pthread_detach(td[READER_THREAD]);
		error_handler("pthread_detach()", rc);
		rc = pthread_detach(td[WRITER_THREAD]);
		error_handler("pthread_detach()", rc);
		close(serv_sock);
		exit(1);
	}

	// 정말 종료할 것인지 묻기 위해
	// 진행 중이던 라이터 스레드를 종료시킨다.
	rc = pthread_cancel(td[WRITER_THREAD]);
	error_handler("pthread_cancel()", rc);
	if(sig == SIGINT){
		while(1){
			printf("채팅을 종료하시겠니까?(Y\\N)\n");
			fgets(msg, MAX_BUFFER_SIZE, stdin);
			msg[strlen(msg) - 1] = '\0';
			if(strncmp(msg, "Y", 1) == 0){
				write(serv_sock, exit_msg, MAX_BUFFER_SIZE);
				rc = pthread_detach(td[READER_THREAD]);
				error_handler("pthread_detach()", rc);
				close(serv_sock);
				exit(1);
			}
			else if(strncmp(msg, "N", 1) == 0) break;
		}
	}
	// 종료하지 않는 경우 다시 라이터 스레드를 생성한다.
	rc = pthread_create(&td[WRITER_THREAD], NULL, write_thread, (void*)&serv_sock);
	error_handler("pthread_create()->write thread", rc);
}

void error_handler(char* msg, int rv){
	if(rv){
		fprintf(stderr, "error msg : %s\n", msg);
		exit(1);
	}
}
