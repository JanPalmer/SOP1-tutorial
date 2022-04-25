
#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <sys/un.h>
#include <signal.h>
#include <netdb.h>
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))

#define MIN_NUM 1
#define MAX_NUM 1000
#define TRIES_NUM 3

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int make_socket(char* name , struct sockaddr_un *addr){
	int socketfd;
	if((socketfd = socket(PF_UNIX,SOCK_STREAM,0))<0) ERR("socket");
	memset(addr, 0, sizeof(struct sockaddr_un));
	addr->sun_family = AF_UNIX;
	strncpy(addr->sun_path,name,sizeof(addr->sun_path)-1);
	return socketfd;
}

int connect_socket(char *name){
	struct sockaddr_un addr;
	int socketfd;
	socketfd = make_socket(name,&addr);
	if(connect(socketfd,(struct sockaddr*) &addr,SUN_LEN(&addr)) < 0){
		if(errno!=EINTR) ERR("connect");
		else { 
			fd_set wfds ;
			int status;
			socklen_t size = sizeof(int);
			FD_ZERO(&wfds);
			FD_SET(socketfd, &wfds);
			if(TEMP_FAILURE_RETRY(select(socketfd+1,NULL,&wfds,NULL,NULL))<0) ERR("select");
			if(getsockopt(socketfd,SOL_SOCKET,SO_ERROR,&status,&size)<0) ERR("getsockopt");
			if(0!=status) ERR("connect");
		}
	}
	return socketfd;
}
ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		if(c<0) return c;
		if(0==c) return len;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}
ssize_t bulk_write(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}
void print_answer(int32_t data){
    printf("%d\n", ntohl(data));	
}
void usage(char * name){
	fprintf(stderr,"USAGE: %s socket \n",name);
}

void do_client(int fd)
{
    int32_t rand_num;
    int32_t data;
    int32_t* p_data = &data;	
    int counter = 0;
    struct timespec ts;
    ts.tv_nsec = 750*1000*1000;
    ts.tv_sec = 2; // Debugging

    srand(getpid());
    do
    {
        rand_num = rand()%(MAX_NUM-MIN_NUM)+MIN_NUM;
        *p_data = htonl(rand_num);
        printf("Client: Sent: %d\n", rand_num);
	    if(bulk_write(fd,(char *)p_data,sizeof(int32_t))<0) ERR("write");        

	    if(bulk_read(fd,(char *)p_data,sizeof(int32_t))<(int)sizeof(int32_t)) ERR("read");
	    if(rand_num == ntohl(*p_data))
            printf("HIT\n");

        counter++;
        
        nanosleep(&ts, NULL);
    }while(counter < TRIES_NUM);
	
}

int main(int argc, char** argv) {
	int fd;
	if(argc!=2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	fd=connect_socket(argv[1]);
	do_client(fd);
    if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");
	return EXIT_SUCCESS;
}
