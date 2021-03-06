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
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))

#define BACKLOG 3
#define MIN_NUM 1
#define MAX_NUM 1000
#define MAX_CLIENTS 2

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
int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}
int bind_local_socket(char *name){
 	struct sockaddr_un addr;
 	int socketfd;
 	if(unlink(name) <0&&errno!=ENOENT) ERR("unlink");
 	socketfd = make_socket(PF_UNIX,SOCK_STREAM);
 	memset(&addr, 0, sizeof(struct sockaddr_un));
 	addr.sun_family = AF_UNIX;
 	strncpy(addr.sun_path,name,sizeof(addr.sun_path)-1);
 	if(bind(socketfd,(struct sockaddr*) &addr,SUN_LEN(&addr)) < 0)  ERR("bind");
 	if(listen(socketfd, BACKLOG) < 0) ERR("listen");
 	return socketfd;
}
int bind_tcp_socket(uint16_t port){
	struct sockaddr_in addr;
	int socketfd,t=1;
	socketfd = make_socket(PF_INET,SOCK_STREAM);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t))) ERR("setsockopt");
	if(bind(socketfd,(struct sockaddr*) &addr,sizeof(addr)) < 0)  ERR("bind");
	if(listen(socketfd, BACKLOG) < 0) ERR("listen");
	return socketfd;
}
void usage(char * name){
	fprintf(stderr,"USAGE: %s socket port\n",name);
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

void process(int32_t *data)
{
    static int32_t max_guess = MIN_NUM-1; 
    int32_t num = ntohl(*data);
    printf("Server: Received: %d\n", num);
    if(num > max_guess)
        max_guess = num;
    *data = htonl(max_guess);    
    printf("Server: Max guess: %d\n", max_guess);

}
int add_new_client(int sfd, int clients[MAX_CLIENTS]){
	int nfd;
    struct sockaddr address;
    ssize_t addrlen;

	if((nfd=TEMP_FAILURE_RETRY(accept(sfd, (struct sockaddr *)&address, (socklen_t*)&addrlen)))<0) {
		if(EAGAIN==errno||EWOULDBLOCK==errno) return -1;
		ERR("accept");
	}
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i] == 0)
        {
            clients[i] = nfd;
            break;
        }	
    }
    return nfd;
}

void communicate(int *p_cfd, int* p_active_clients){
	ssize_t size;	
    int32_t data;
    int32_t* p_data = &data;
  
    if((size=bulk_read(*p_cfd,(char*)p_data,sizeof(int32_t)))<0) ERR("read");
	    
    if(size == 0)
    {
        printf("Server: Client disconnected\n");
        if(TEMP_FAILURE_RETRY(close(*p_cfd))<0)ERR("close");
        *p_cfd = 0; 
        (*p_active_clients)--;
    }
	else if(size==(int)sizeof(int32_t)){
	    process(p_data);
        if(bulk_write(*p_cfd,(char*)p_data,sizeof(int32_t))<0&&errno!=EPIPE) ERR("write");
    }
	
}
void do_server(int master_fd){
	int cfd, max_fd = master_fd;
    fd_set base_rfds, rfds;
	sigset_t mask, oldmask;
    int clients[MAX_CLIENTS];
    int active_clients = 0;

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        clients[i] = 0;
    }            

	FD_ZERO(&base_rfds);
	FD_SET(master_fd, &base_rfds);
	sigemptyset (&mask);
	sigaddset (&mask, SIGINT);
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	while(do_work){
		rfds=base_rfds;
        max_fd = master_fd;
        for(int i=0;i<MAX_CLIENTS;i++)
        {
            if(clients[i] > 0)
                FD_SET(clients[i], &rfds);
            if(clients[i] > max_fd)
                max_fd = clients[i];
        }            
		if(pselect(max_fd+1,&rfds,NULL,NULL,NULL,&oldmask)>0){ // Co je??eli clients zape??nione
            if(active_clients != MAX_CLIENTS && FD_ISSET(master_fd, &rfds)) // New connection		                
            {
                cfd=add_new_client(master_fd,clients);                        
                active_clients++;
            }
            for(int i=0;i<MAX_CLIENTS;i++) // Already connected socket
            {
                cfd = clients[i];
                if(cfd == 0 || !FD_ISSET(cfd, &rfds)) continue;
                communicate(&clients[i], &active_clients);
            }
		}else{
			if(EINTR==errno) continue;
			ERR("pselect");
		}
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
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
	
	//fdT=bind_tcp_socket(atoi(argv[1]));
    fdT = bind_local_socket(argv[1]);
	new_flags = fcntl(fdT, F_GETFL) | O_NONBLOCK;
	fcntl(fdT, F_SETFL, new_flags);

	do_server(fdT);

	if(TEMP_FAILURE_RETRY(close(fdT))<0)ERR("close");
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}

