#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include "socklib.c"
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))
#define MAX(a, b) ((a > b) ? (a) : (b))
#define REQUEST_NUMBER 3

int number_of_numbers = 0;
volatile sig_atomic_t do_work=1 ;
void sigint_handler(int sig) {
	do_work=0;
}
int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}
int32_t max(int32_t a, int32_t b){
	return ((a > b) ? (a) : (b));
}

void communicate(int cfd, int32_t *maximum){
	ssize_t size;
	int32_t data;
	// for(int i = 0; i < REQUEST_NUMBER; i++){
	// 	if((size = bulk_read(cfd,(char *)&data,sizeof(int32_t)))<0) ERR("read:");
	// 	//printf("server: read, size = [%ld]\n", size);
	// 	if(size == (int)sizeof(int32_t)){
	// 		printf("%d\n", ntohl(data));
	// 		*maximum = max(ntohl(data), *maximum);
	// 		data = htonl(*maximum);
	// 		if(bulk_write(cfd,(char *)&data,sizeof(int32_t))<0 && errno!=EPIPE) ERR("write:");
	// 		//printf("server: write\n");
	// 	}		
	// }

	if((size = bulk_read(cfd,(char *)&data,sizeof(int32_t)))<0) ERR("read:");
	//printf("server: read, size = [%ld]\n", size);
	if(size == (int)sizeof(int32_t)){
		data = ntohl(data);
		//printf("server: %d\n", data);
		*maximum = max(data, *maximum);
		//printf("server: %d\n", *maximum);
		data = htonl(*maximum);
		if(bulk_write(cfd,(char *)&data,sizeof(int32_t))<0 && errno!=EPIPE) ERR("write:");
		//printf("server: write\n");
	}	

	if(TEMP_FAILURE_RETRY(close(cfd))<0)ERR("close");
	//printf("Server: connection closed\n");		
}
void doServer(int fdT){
	int cfd,fdmax;
	fd_set base_rfds, rfds;
	sigset_t mask, oldmask;
	FD_ZERO(&base_rfds);
	FD_SET(fdT, &base_rfds);
	fdmax=fdT;
	sigemptyset (&mask);
	sigaddset (&mask, SIGINT);
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	int32_t max = -1000;

	while(do_work){
		rfds=base_rfds;
		//printf("spinnin\n");
		if(pselect(fdmax+1,&rfds,NULL,NULL,NULL,&oldmask)>0){
			cfd=add_new_client(fdT);

			//printf("server: New client\n");
			if(cfd>=0)communicate(cfd, &max);
			number_of_numbers++;
            //break;
		}else{
			if(EINTR==errno) continue;
			ERR("pselect");
		}
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
	printf("server: numbers received: %d\n", number_of_numbers);
}
int main(int argc, char** argv) {
	int fdT;
	int new_flags;
	if(argc!=3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
	fdT=bind_tcp_socket(atoi(argv[2]));
	new_flags = fcntl(fdT, F_GETFL) | O_NONBLOCK;
	fcntl(fdT, F_SETFL, new_flags);
	
	doServer(fdT);

	if(TEMP_FAILURE_RETRY(close(fdT))<0)ERR("close");
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}