//#include <bits/pthreadtypes.h>
//#include <bits/pthreadtypes.h>
#define _GNU_SOURCE
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define BACKLOG 3
#define CHUNKSIZE 500
#define NMMAX 30
#define THREAD_NUM 3
volatile sig_atomic_t dowork = 1;

typedef struct arguments {
	int socket;
	struct sockaddr_in addr;
	sem_t *semaphore;
    pthread_barrier_t *bar;
    int triplet;
    pthread_mutex_t* pMutex;
} thread_arg;


void usage(char *name)
{
	fprintf(stderr, "USAGE: %s port limit\n", name);
	exit(EXIT_FAILURE);
}

int make_socket(int domain, int type)
{
	int sock;
	sock = socket(domain, type, 0);
	if (sock < 0)
		ERR("socket");
	return sock;
}

void *threadfunc(void *arg)
{
    char buf[10];
    socklen_t size = sizeof(struct sockaddr_in);
	thread_arg targ;
	memcpy(&targ, arg, sizeof(targ));
    pthread_barrier_wait(targ.bar);

    while(1)
    {
        memset(buf,0,10);
        if (recvfrom(targ.socket, buf, sizeof(buf), 0, &targ.addr, &size) < 0) {
		    if (errno == EINTR) continue;
            ERR("recvfrom:");
        }
        if(strcmp(buf,"") == 0) break;
        printf("Got: %s",buf);
        if(strcmp(buf,"rock\r\n") == 0) 
            break;
        if(strcmp(buf,"paper\n") == 0) 
            break;
        if(strcmp(buf,"scissors\n") == 0) 
            break;
        if(strcmp(buf,"Spock\n") == 0) 
            break;          
        if(strcmp(buf,"lizard\n") == 0) 
            break;  
    }

    if(pthread_barrier_destroy(targ.bar) < 0)
    {
        if(errno != EINVAL)
        ERR("pthread_barrier_destroy");
    }
	if (sem_post(targ.semaphore) == -1)
		ERR("sem_post");
	return NULL;
}
struct sockaddr_in make_address(char *address, char *port)
{
	int ret;
	struct sockaddr_in addr;
	struct addrinfo *result;
	struct addrinfo hints = {};
	hints.ai_family = AF_INET;
	if ((ret = getaddrinfo(address, port, &hints, &result))) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	addr = *(struct sockaddr_in *)(result->ai_addr);
	freeaddrinfo(result);
	return addr;
}


int bind_tcp_socket(char * port)
{
	struct sockaddr_in addr;
	int socketfd, t = 1;
	socketfd = make_socket(PF_INET, SOCK_STREAM);
	memset(&addr, 0x00, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
		ERR("setsockopt");
	if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		ERR("bind");
	if (listen(socketfd, BACKLOG) < 0)
		ERR("listen");
	return socketfd;
}

int add_new_client(int sfd)
{
	int nfd;
	if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0) {
		if (EAGAIN == errno || EWOULDBLOCK == errno)
			return -1;
		ERR("accept");
	}
	return nfd;
}

void doServer(int socket, int limit)
{
    int16_t time;
    int clientSocket;
    int triplets = 0;
    int triplet_id = -1;
	int16_t deny = -1;
	deny = htons(deny);
	pthread_t thread;
    pthread_mutex_t mutex;

    pthread_barrier_t* bars;


	struct sockaddr_in addr;
	struct arguments *args;
	socklen_t size = sizeof(struct sockaddr_in);
	sem_t semaphore;
	if (sem_init(&semaphore, 0, limit) != 0)
		ERR("sem_init");
	while (dowork) {
        if ((clientSocket = add_new_client(socket)) == -1)
			continue;
        if (TEMP_FAILURE_RETRY(sem_trywait(&semaphore)) == -1) {
			switch (errno) {
			case EAGAIN:
                close(clientSocket);
			case EINTR:
				continue;
			}
			ERR("sem_wait");
		}

        if(triplets == 0)
        {
            triplet_id++;
            triplets = (triplets+1)%3;
            if((bars = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t))) == NULL) ERR("malloc");
            pthread_barrier_init(bars, NULL, 3);
        }
		if ((args = (struct arguments *)malloc(sizeof(struct arguments))) == NULL)
			ERR("malloc:");
		args->socket = clientSocket;
        args->triplet = triplet_id;
        args->bar = bars;
		args->addr = addr;
		args->semaphore = &semaphore;
		if (pthread_create(&thread, NULL, threadfunc, (void *)args) != 0)
			ERR("pthread_create");
		if (pthread_detach(thread) != 0)
			ERR("pthread_detach");
	}
}

int main(int argc, char **argv)
{
    int limit, socket, new_flags;
    if (argc != 3)
		usage(argv[0]);
    if((limit = atoi(argv[2])) == 0)
    	usage(argv[0]);
	pthread_t* thread;
    if((thread = (pthread_t *)malloc(sizeof(pthread_t)*limit)) == NULL) ERR("malloc");
	thread_arg* targ;    
    if((targ = (thread_arg *)malloc(sizeof(thread_arg)*limit)) == NULL) ERR("malloc");

	socket = bind_tcp_socket(argv[1]);
	new_flags = fcntl(socket, F_GETFL) | O_NONBLOCK;
	if (fcntl(socket, F_SETFL, new_flags) == -1)    ERR("fcntl");
    doServer(socket, limit);
	for (int i = 0; i < THREAD_NUM; i++)
		if (pthread_join(thread[i], NULL) != 0)
			ERR("pthread_join");
	if (TEMP_FAILURE_RETRY(close(socket)) < 0)
		ERR("close");
    free(thread);
    free(targ);
	return EXIT_SUCCESS;
}
