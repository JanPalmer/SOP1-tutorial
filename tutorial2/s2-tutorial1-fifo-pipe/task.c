#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <signal.h>

#define ERR(source) (perror(source),\
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
                    exit(EXIT_FAILURE))

// MAX_BUFF must be in one byte range
#define PROC_NUMBER 3 // parent + children
#define BUFSIZE 5
#define TEMP_FAILURE_RETRY_mod(expression)                                         \
  (__extension__({                                                             \
    long int __result;                                                         \
    do                                                                         \
      __result = (long int)(expression);                                       \
    while (__result == -1L && errno == EINTR && errno != EPIPE);                                 \
    __result;                                                                  \
  }))

volatile sig_atomic_t last_signal = 0;

struct fd{
    int fds[2];
};

void swap(int* a, int* b){
    int tmp = *a;
    *a = *b;
    *b = tmp;
    return;
}

int sethandler(void (*f)(int), int sigNo){
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if(-1 == sigaction(sigNo, &act, NULL))
        return -1;

    return 0;
}

void sig_handler(int sig){
    last_signal = sig;
}

void sig_killme(int sig){
    exit(EXIT_SUCCESS);
}

void sigchld_handler(int sig){
    pid_t pid;
    for(;;){
        pid = waitpid(0, NULL, WNOHANG);
        if(0 == pid) return;
        if(0 >= pid) {
            if(ECHILD == errno) return;
            ERR("waipid");
        }
    }
}

void child_work(struct fd *pipeFD, int procNum){
    //printf("[%d] Lil boy\n", procNum);
    srand(getpid());
    char buf[BUFSIZE];
    int num;
    int ret;
    if(sethandler(sig_handler, SIGINT)) ERR("Setting SIGINT handler in child");
    //if(sethandler(sig_handler, SIGPIPE)) ERR("Setting SIGPIPE handler in child");  
    // if(TEMP_FAILURE_RETRY(read(pipeFD->fds[0], &buf, 3)) < 1) ERR("read");
    // printf("%s, pid: %d\n", buf, getpid());
    // newNum = rand()%99;
    // sprintf(buf, "%02d", newNum);
    // if(TEMP_FAILURE_RETRY(write(pipeFD->fds[1], &buf, 3)) < 1) ERR("write");

    while(last_signal != SIGINT){
        printf("[%d] Trying to read\n", getpid());
        if((ret = read(pipeFD->fds[0], &buf, BUFSIZE)) < 0) ERR("read");
        if(ret == 0) break;

        printf("[%d] %s\n", getpid(), buf);
        num = atoi(buf);
        if(num == 0) break;

        num = num + (rand()%21 - 10);
        sprintf(buf, "%04d", num);
        sleep(1);
        printf("[%d] Trying to write\n", getpid());           
        if(TEMP_FAILURE_RETRY(write(pipeFD->fds[1], &buf, BUFSIZE)) < 1) ERR("write");
    }

    //kill(0, SIGINT);
    if (TEMP_FAILURE_RETRY(close(pipeFD->fds[0]))) ERR("close");
    if (TEMP_FAILURE_RETRY(close(pipeFD->fds[1]))) ERR("close");

    return;
}

void parent_work(struct fd *pipeFD){
    //printf("[0] Chad parent\n");
    srand(getpid());
    char buf[BUFSIZE];
    int num;
    int ret;
    if(sethandler(sig_handler, SIGINT)) ERR("Setting SIGINT handler in parent");
    //if(sethandler(sig_handler, SIGPIPE)) ERR("Setting SIGPIPE handler in parent");

    sprintf(buf, "%04d", 0);
    if(TEMP_FAILURE_RETRY(write(pipeFD->fds[1], &buf, BUFSIZE)) < 0) ERR("write");

    while(last_signal != SIGINT){
        printf("[%d] Trying to read - parent\n", getpid());
        if((ret = read(pipeFD->fds[0], &buf, BUFSIZE)) < 0) ERR("read");
        if(ret == 0) break;
        
        printf("[%d] %s\n", getpid(), buf);
        num = atoi(buf);
        if(num == 0) break;;

        num = num + (rand()%21 - 10);
        sprintf(buf, "%04d", num);
        sleep(1);
        printf("[%d] Trying to write - parent\n", getpid());          
        if(TEMP_FAILURE_RETRY(write(pipeFD->fds[1], &buf, BUFSIZE)) < 1) ERR("write");
    }

    //kill(0, SIGINT);
    if (TEMP_FAILURE_RETRY(close(pipeFD->fds[0]))) ERR("close");
    if (TEMP_FAILURE_RETRY(close(pipeFD->fds[1]))) ERR("close");

    return;
}

void create_pipes_and_children(struct fd *pipeFD){
    // function creates all pipes, then distributes only the necessary ones to each child and parent
    // creates a loop by shifting 'pipe out's by one
    struct fd pipesAll[PROC_NUMBER]; // all pipe descriptors
    for(int i = 0; i < PROC_NUMBER; i++)
        if(pipe(pipesAll[i].fds) < 0) ERR("pipe");

    int tmpfd = pipesAll[PROC_NUMBER - 1].fds[0];
    for(int i = 0; i < PROC_NUMBER; i++){
        swap(&tmpfd, &pipesAll[i].fds[0]);
        //printf("%d - iteration\n", i);
        if(i == 0) continue; // skip parent       
        switch(fork()){
            case 0:
                pipeFD->fds[0] = pipesAll[i].fds[0];
                pipeFD->fds[1] = pipesAll[i].fds[1];
                child_work(pipeFD, i);
                // if (TEMP_FAILURE_RETRY(close(pipeFD->fds[0]))) ERR("close");
                // if (TEMP_FAILURE_RETRY(close(pipeFD->fds[1]))) ERR("close");
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                break;
        }
    }

    // deal with the parent
    pipeFD->fds[0] = pipesAll[0].fds[0];
    pipeFD->fds[1] = pipesAll[0].fds[1];
    // parent work? or in main after return
    parent_work(pipeFD);
    // for(int i = 0; i < PROC_NUMBER; i++){
    //     if(TEMP_FAILURE_RETRY(close(pipesAll[i].fds[0]))) ERR("close");
    //     if(TEMP_FAILURE_RETRY(close(pipesAll[i].fds[1]))) ERR("close");
    // }
    while(wait(NULL) > 0);

    return;
}

void usage(char* name){
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "0<n<=10 - number of children\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv){
    int n;
    struct fd pd;

    if(sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE handler");
    if(sethandler(sigchld_handler, SIGCHLD)) ERR("Setting parent SIGCHLD");
    create_pipes_and_children(&pd);

    printf("Death of a parent\n");    
    return EXIT_SUCCESS;
}