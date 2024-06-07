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
#define SERVER_SIGINT 100
char user_name[MAX_BUFFER_SIZE];
int serv_sock;
pthread_t td[2];

void error_handler(char* msg, int rv);
void* read_thread(void* argc);
void* write_thread(void* argc);
void signal_handler(int sig);

int main(int argc, char* argv[]){
	int rc;
	char tmp[MAX_BUFFER_SIZE];
	struct sockaddr_in serv_addr;
	signal(SIGINT, signal_handler);
	if(argc != 4){
		fprintf(stderr, "usage : ./client <server IP> <server port> <user name>\n");
		exit(1);
	}
	strcpy(user_name, argv[3]);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1) error_handler("socket", serv_sock);
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));
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

	rc = pthread_create(&td[0], NULL, write_thread, (void*)&serv_sock);
	error_handler("pthread_create()->write thread", rc);
	rc = pthread_create(&td[1], NULL, read_thread, (void*)&serv_sock);
	error_handler("pthread_create()->read thread", rc);

	rc = pthread_join(td[0], NULL);
	error_handler("pthread_join()", rc);
	rc = pthread_join(td[1], NULL);
	error_handler("pthread_join()", rc);

	return 0;
}

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

void* write_thread(void* arg){
	int rc;
	char msg[MAX_BUFFER_SIZE];
	char tmp[MAX_BUFFER_SIZE];

	while(1){
		fgets(msg, MAX_BUFFER_SIZE, stdin);
		msg[strlen(msg) - 1] = '\0';
		strcpy(tmp, user_name);
		strcat(tmp, " : ");
		strcat(tmp, msg);
		rc = write(serv_sock, tmp, strlen(tmp));
		if(rc < 0) error_handler("write()", rc);
		tmp[0] = '\0';
	}
}

void signal_handler(int sig){
	char msg[MAX_BUFFER_SIZE];
	char exit_msg[MAX_BUFFER_SIZE];
	int rc;
	rc = pthread_cancel(td[0]);
	error_handler("pthread_cancel()", rc);
	strcpy(exit_msg, "q");

	if(sig == SIGINT){
		while(1){
			printf("채팅을 종료하시겠니까?(Y\\N)\n");
			fgets(msg, MAX_BUFFER_SIZE, stdin);
			msg[strlen(msg) - 1] = '\0';
			if(strncmp(msg, "Y", 1) == 0){
				write(serv_sock, exit_msg, MAX_BUFFER_SIZE);
				pthread_detach(td[1]);
				close(serv_sock);
				exit(1);
			}
			else if(strncmp(msg, "N", 1) == 0) break;
		}
	}

	if(sig == SERVER_SIGINT){
		printf(" ==== 서버 종료 ====\n");
		write(serv_sock, exit_msg, MAX_BUFFER_SIZE);
		pthread_detach(td[1]);
		close(serv_sock);
		exit(1);
	}

	rc = pthread_create(&td[0], NULL, write_thread, (void*)&serv_sock);
	error_handler("pthread_create()->write thread", rc);
}

void error_handler(char* msg, int rv){
	if(rv){
		fprintf(stderr, "error msg : %s\n", msg);
		exit(1);
	}
}
