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
#include <arpa/inet.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define BACKLOG 3
#define CHUNKSIZE 500
#define NMMAX 30
#define THREAD_NUM 3
#define BUFSIZE 40
volatile sig_atomic_t dowork = 1;

typedef struct arguments {
	int socket;
	int lead;
	pthread_cond_t *cond;
    pthread_barrier_t *bar;
    int pair_id;
    pthread_mutex_t* pMutex;
	int* ShouldClose;
	sem_t* semaphore;
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

int bind_tcp_socket(char *address, char * port)
{
	struct sockaddr_in addr;
	int socketfd, t = 1;
	socketfd = make_socket(PF_INET, SOCK_STREAM);
	memset(&addr, 0x00, sizeof(struct sockaddr_in));
    addr = make_address(address, port);
	// addr.sin_family = AF_INET;
	// addr.sin_port = htons(atoi(port));
	// addr.sin_addr.s_addr = 
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

void* threadfunc(void *arg)
{
    char buf[BUFSIZE];
    // socklen_t size = sizeof(struct sockaddr_in);
	thread_arg* targ = (thread_arg*) arg;
	memset(buf, 0, BUFSIZE);
	sprintf(buf, "Waiting in pair [%d], lead: [%d]\n", targ->pair_id, targ->lead);
	send(targ->socket, buf, BUFSIZE, 0);
	printf("%s", buf);

    pthread_barrier_wait(targ->bar);
    int recv_return;

    while(targ->ShouldClose)
    {
        memset(buf,0, BUFSIZE);
        recv_return = recv(targ->socket, buf, BUFSIZE, 0);
        if (recv_return < 0) {
		    if (errno == EINTR) continue;
            ERR("recvfrom:");
        }
        if(recv_return == 0){
            printf("[%d] Pair: [%d] Other client left, ending\n", gettid(), targ->pair_id);
			*(targ->ShouldClose) = 0;
            break;
        }

        if(strcmp(buf,"") == 0) break;
        printf("Got: %s",buf);
        if(strcmp(buf,"stand\r\n") == 0){
            printf("[%d] stand. Ending\n", gettid());
            break;            
        }
        if(strcmp(buf,"hit") == 0){
            printf("[%d] hit\n", gettid());
            continue;
        } 
    }

	pthread_barrier_wait(targ->bar);

	printf("Pair [%d] ending\n", targ->pair_id);

	sem_post(targ->semaphore);
	if(targ->lead == 0){
		shutdown(targ->socket, SHUT_RDWR);
		close(targ->socket);
		free(targ);
		return NULL;
	}

    if(pthread_barrier_destroy(targ->bar) < 0)
    {
        if(errno != EINVAL)
        ERR("pthread_barrier_destroy");
    }
    if(pthread_cond_destroy(targ->cond) != 0) ERR("cond_destroy");

	shutdown(targ->socket, SHUT_RDWR);
	close(targ->socket);
	//int semvalue;
	// sem_getvalue(targ->semaphore, &semvalue);
	// printf("Clients left: %d", semvalue);
	//sem_post(targ->semaphore);
	free(targ->ShouldClose);
	free(targ->bar);
	free(targ->cond);
	free(targ);
	return NULL;
}

void doServer(int socket, int limit)
{
    int clientSocket;
    int pairs = 0;
    int pair_id = -1;
	int16_t deny = -1;
	deny = htons(deny);
	pthread_t thread;
    pthread_barrier_t* bars;
    pthread_cond_t* condtmp;
	int* sharedClose;
	char tmpbuf[BUFSIZE];
	memset(tmpbuf, 0, BUFSIZE);
	strcpy(tmpbuf, "Too many clients\n");

	struct arguments *args;
	sem_t semaphore;
	if (sem_init(&semaphore, 0, limit) != 0)
		ERR("sem_init");
	while (dowork) {
        if ((clientSocket = add_new_client(socket)) == -1)
			continue;
        if (TEMP_FAILURE_RETRY(sem_trywait(&semaphore)) == -1) {
			switch (errno) {
			case EAGAIN:
				send(clientSocket, tmpbuf, BUFSIZE, 0);
                close(clientSocket);
			case EINTR:
				continue;
			}
			ERR("sem_wait");
		}

        if(pairs == 0)
        {
            pair_id++;
            pairs = (pairs + 1) % 2;
            
            if((bars = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t))) == NULL) ERR("malloc");
            pthread_barrier_init(bars, NULL, 2);

            if((condtmp = (pthread_cond_t*)malloc(sizeof(pthread_cond_t))) == NULL) ERR("malloc cond");

			if((sharedClose = (int*)malloc(sizeof(int))) == NULL) ERR("malloc int");
			*sharedClose = 1;
        }
		else {
		pairs = 0;
		}

        if((args = (thread_arg*)malloc(sizeof(thread_arg))) == NULL) ERR("malloc arg");

		if ((args = (struct arguments *)malloc(sizeof(struct arguments))) == NULL)
			ERR("malloc:");
		args->socket = clientSocket;
        args->pair_id = pair_id;
        args->bar = bars;
        args->cond = condtmp;
		args->ShouldClose = sharedClose;
		args->semaphore = &semaphore;
		if(pairs == 0)
			args->lead = 1;
		else
		 	args->lead = 0;
        //args->addr = addr;
        if (pthread_create(&thread, NULL, threadfunc, (void *)args) != 0)
          ERR("pthread_create");
        if (pthread_detach(thread) != 0)
          ERR("pthread_detach");
	}

	sem_destroy(&semaphore);
}

int main(int argc, char **argv)
{
    int limit, socket, new_flags;
    if (argc != 4)
		usage("%s limit, address, port");
    if((limit = atoi(argv[1])) <= 0){
        usage("Inputted player count is less or equal 0");
    }

	// pthread_t* thread;
    // if((thread = (pthread_t *)malloc(sizeof(pthread_t)*limit)) == NULL) ERR("malloc");
	// thread_arg* targ;    
    // if((targ = (thread_arg *)malloc(sizeof(thread_arg)*limit)) == NULL) ERR("malloc");

	socket = bind_tcp_socket(argv[2], argv[3]);
	new_flags = fcntl(socket, F_GETFL) | O_NONBLOCK;
	if (fcntl(socket, F_SETFL, new_flags) == -1)    ERR("fcntl");
    doServer(socket, limit);

	// for (int i = 0; i < THREAD_NUM; i++)
	// 	if (pthread_join(thread[i], NULL) != 0)
	// 		ERR("pthread_join");
	if (TEMP_FAILURE_RETRY(close(socket)) < 0)
		ERR("close");
    // free(thread);
    // free(targ);
	return EXIT_SUCCESS;
}