#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include "socklib.c"
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))
#define REQUEST_NUMBER 3
#define SLEEP_TIME 750


int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

void prepare_request(int32_t *data){
    srand(time(NULL));
	*data = htonl(rand()%1000 + 1);
}

void print_answer(int32_t *data){
	printf("%d", ntohl(*data));
}

void send_logic(char** argv){
	int fd;
	int32_t data, answer;
	srand(getpid());
	for(int i = 0; i < REQUEST_NUMBER; i++){
		//printf("[%d] Try number: %d\n", getpid(), i+1);
		fd=connect_socket_tcp(argv[1],argv[2]);
		data = rand()%1000 + 1;
		//printf("[%d] %d\n", getpid(), data);
		data = htonl(data);
		if(bulk_write(fd, (char*)&data,sizeof(int32_t))<0 && errno!=EPIPE) ERR("client write:");
		//printf("[%d] client: write\n", getpid());
		if(bulk_read(fd, (char*)&answer,sizeof(int32_t))<(int)sizeof(int32_t)) ERR("client read:");
		//printf("[%d] client: read\n", getpid());
		answer = ntohl(answer);
		data = ntohl(data);
		//printf("[%d] %d == %d\n", getpid(), answer, data);
		if(answer == data){
			printf("[%d] HIT\n", getpid());
		}
		if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");
		msleep(SLEEP_TIME);
	}
}

int main(int argc, char** argv) {
	if(argc!=3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	send_logic(argv);
	return EXIT_SUCCESS;
}