#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <signal.h>
#include <netdb.h>
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))

#define BLOCK_SIZE 10
#define MAX_LENGTH 2000
#define MAX_ANSWER_LENGTH 100
#define STDIN_FD 0

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

void usage(char * name){
	fprintf(stderr,"USAGE: %s socket operand1 operand2 operation \n",name);
}

ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		if(c<0 )
        {
            if(errno != EAGAIN)
                return c;
            continue;
        } 
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
		if(c<0 )
        {
            if(errno != EAGAIN)
                return c;
            continue;
        } 
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

void do_client(int fd)
{
    char question[MAX_LENGTH];
	char connection_ack[4];
    char data[BLOCK_SIZE];
    size_t offset = 0, size;
    char answer[MAX_ANSWER_LENGTH];
    fd_set base_rfds, rfds;
	int b_ready = 0;

	FD_ZERO(&base_rfds);
	FD_SET(fd, &base_rfds);
	FD_SET(STDIN_FD, &base_rfds); //stdin
    
	if(bulk_read(fd,connection_ack,4)<0 && errno != EPIPE) ERR("read:"); // req
	if(0 == strncmp(connection_ack,"NIE",4)) 
	{
		printf("[Client:%d] Rejected\n",getpid());
		return;
	}

	while(1)
    {
		rfds = base_rfds;
        memset(answer, 0, MAX_ANSWER_LENGTH); // Control byte
        if(bulk_write(fd,answer,MAX_ANSWER_LENGTH)<0 && errno != EPIPE) ERR("write:"); // req
        if(errno == EPIPE) break;

		if(select(fd+1,&rfds,NULL,NULL,NULL)>0)
		{ 
			if(FD_ISSET(fd, &rfds))
			{
				if((size = bulk_read(fd,(char *)data,sizeof(data))) < 0) ERR("read:");
				if(size == 0) break;

				for(int i=0;i<BLOCK_SIZE;i++)
				{
					if(data[i] == 0)
					{
						question[offset] = '\0';
						offset = 0;
						break;
					}
					else
						question[offset++] = data[i];
				}
				if(offset == 0) // Reset so question ready
					b_ready = 1;
			}
			if(FD_ISSET(STDIN_FD, &rfds) || b_ready == 1)
			{
				if(b_ready)
				{
					printf("[Client:%d] %s\n", getpid(), question);
					fgets(answer, MAX_ANSWER_LENGTH, stdin);
					if(errno == EINTR) return;
					if(bulk_write(fd,answer,MAX_ANSWER_LENGTH)<0 && errno != EPIPE) ERR("write:"); // answer    
					b_ready = 0; 
				}
				else
            	{
					printf("[Client:%d] Not yet!\n", getpid());
					fgets(answer, MAX_ANSWER_LENGTH, stdin);
				}
			}
		}        
    }
}

int main(int argc, char** argv) {
	int fd;
	if(argc!=2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Setting SIGPIPE:");
	//if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
	fd=connect_socket(argv[1]);
    do_client(fd);
	if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");
	return EXIT_SUCCESS;
}

