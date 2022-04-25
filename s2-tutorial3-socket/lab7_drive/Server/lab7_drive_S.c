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
#include <time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))

#define BACKLOG 3
#define MAX_CLIENTS 2
#define BLOCK_SIZE 10
#define MAX_ANSWER_LENGTH 100
#define MAX_LENGTH 2000

volatile sig_atomic_t do_work=1 ;

typedef struct socket_log
{
    int socket_fd;
    int question_no;
    int question_offset;
} socket_log_t;

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
	fprintf(stderr,"USAGE: %s socket questions_file\n",name);
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
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}


int add_new_client(int sfd, socket_log_t clients[MAX_CLIENTS], int active_clients){
	int nfd;
    struct sockaddr address;
    ssize_t addrlen;

	if((nfd=TEMP_FAILURE_RETRY(accept(sfd, (struct sockaddr *)&address, (socklen_t*)&addrlen)))<0) {
		if(EAGAIN==errno||EWOULDBLOCK==errno) return -1;
		ERR("accept");
	}

    if(active_clients >= MAX_CLIENTS)
    {
        if(bulk_write(nfd, "NIE", sizeof("NIE")) < 0 && errno != EPIPE) ERR("write");        
        if(TEMP_FAILURE_RETRY(close(nfd))<0)ERR("close");
        return 0;
    }
    if(bulk_write(nfd, "TAK", sizeof("TAK")) < 0 && errno != EPIPE) ERR("write");

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].socket_fd == 0)
        {
            clients[i].socket_fd = nfd;
            break;
        }	
    }
    return nfd;
}

void prepare_fragment(char data[BLOCK_SIZE], char** questions, int questions_count, socket_log_t* p_client)
{
    int cur = 0;
    int offset = p_client->question_offset;
    char* question = questions[p_client->question_no];
    while(cur < BLOCK_SIZE && offset < MAX_LENGTH && ( offset == 0 || question[offset-1] != 0)) // We want to send the last '\0'
    {
        data[cur++] = question[offset++]; 
    }
    if(offset == MAX_LENGTH || question[offset-1] == 0)
    {
        p_client->question_offset = 0;
        ++(p_client->question_no);
    }
    else
    {
        p_client->question_offset += cur;
    }    
}

void reset_client(socket_log_t* p_client, int* p_active_clients)
{
    printf("[Server] Client disconnected\n");
    if(TEMP_FAILURE_RETRY(close(p_client->socket_fd))<0)ERR("close");
    p_client->socket_fd = 0;
    p_client->question_no = 0;
    p_client->question_offset = 0; 
    (*p_active_clients)--;
}

void communicate(char** questions, int questions_count, socket_log_t* p_client, int* p_active_clients){
	ssize_t size;	
    // Data
    char data[BLOCK_SIZE];
    char answer[MAX_ANSWER_LENGTH];

    int cfd = p_client->socket_fd;        

    if((size=bulk_read(cfd, answer, sizeof(char[MAX_ANSWER_LENGTH]))) < 0) ERR("read"); // receive request or answer
    if(size == 0) 
    {
        reset_client(p_client, p_active_clients);
        return;
    }
    if(answer[0] != 0) // answer
    {
        printf("[Server] received answer to question: %d\n => %s\n",p_client->question_no-1, answer);

        if(p_client->question_no == questions_count)        
            reset_client(p_client, p_active_clients);        
        return;
    }

    prepare_fragment(data, questions, questions_count, p_client);      
    
    if(bulk_write(cfd, data, sizeof(char[BLOCK_SIZE])) < 0 && errno != EPIPE) ERR("write");
    //if(errno == EPIPE) 
    //    reset_client(p_client, p_active_clients);  		
}
void do_server(int master_fd, char** questions, int questions_count){
	int max_fd = master_fd;
    fd_set base_rfds, rfds;
	sigset_t mask, oldmask;
    socket_log_t clients[MAX_CLIENTS];
    int active_clients = 0;
    struct timespec ts = {};
    ts.tv_nsec = 330*1000*1000;

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        clients[i].socket_fd = 0;
        clients[i].question_no = 0;
        clients[i].question_offset = 0;
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
            if(clients[i].socket_fd > 0)
                FD_SET(clients[i].socket_fd, &rfds);
            if(clients[i].socket_fd > max_fd)
                max_fd = clients[i].socket_fd;
        }            
		if(pselect(max_fd+1,&rfds,NULL,NULL,NULL,&oldmask)>0){ 
            if(FD_ISSET(master_fd, &rfds)) // New connection		                
            {
                if(add_new_client(master_fd,clients,active_clients) != 0)
                    active_clients++;
            }
            for(int i=0;i<MAX_CLIENTS;i++) // Already connected socket
            {                
                if(clients[i].socket_fd == 0 || !FD_ISSET(clients[i].socket_fd, &rfds)) continue;
                communicate(questions, questions_count, &clients[i], &active_clients);
                nanosleep(&ts,NULL);
            }
		}else{
			if(EINTR==errno) continue;
			ERR("pselect");
		}
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
}

char** load_questions(char* name, int* length)
{
    char ch;
    int lines = 0; // Last question with '\n'
    size_t size;
    char** questions;
    FILE* file;
    
    if((file = fopen(name,"r")) == NULL) ERR("fopen"); 

    while((ch=fgetc(file))!=EOF) {
      if(ch==';')
         lines++;
    }
    rewind(file);

    questions = (char**)malloc(sizeof(char*) * lines);
    for(int i=0;i<lines;i++)
    {   
        size = sizeof(char) * MAX_LENGTH;     
        questions[i] = (char*)malloc(size);
        memset(questions[i], 0, size);    
        size = getdelim(&questions[i], &size, ';', file); // Hell with MAX_LENGTH
        questions[i][size-1] = 0;
    }

    *length = lines;
    return questions;
}

void free_questions(char** questions, int length)
{
    for(int i=0;i< length; i++)
        free(questions[i]); 
    free(questions);
}

int main(int argc, char** argv) {
	int master_fd;
	int new_flags;
	if(argc!=3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
	
	//master_fd=bind_tcp_socket(atoi(argv[1]));
    master_fd = bind_local_socket(argv[1]);
	new_flags = fcntl(master_fd, F_GETFL) | O_NONBLOCK;
	fcntl(master_fd, F_SETFL, new_flags);

    char** questions;
    int length;
    questions = load_questions(argv[2], &length); 

    for(int i=0;i<length;i++)
    {
        printf("Server: Q%d: %s\n", i + 1, questions[i]);
    }

	do_server(master_fd, questions, length);

	if(TEMP_FAILURE_RETRY(close(master_fd))<0)ERR("close");
    free_questions(questions, length);
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}

