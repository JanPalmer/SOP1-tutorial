#include <asm-generic/errno-base.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#define ERR(source) (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
                    perror(source), kill(0, SIGKILL),\
                    exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if(-1 == sigaction(sigNo, &act, NULL)) ERR("sigaction");
}

void sig_handler(int signum)
{
    printf("[%d] received signal %d\n", getpid(), signum);
    last_signal = signum;
}

void sigchld_handler(int sig)
{
    pid_t pid;
    for(;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if(pid == 0) return;
        if(pid <= 0)
        {
            if(errno == ECHILD) return;
            ERR("waitpid");
        }
    }
}

void child_work(int r)
{
    int t, tt;
    srand(getpid());
    t = rand()%6 + 5;
    while(r-- > 0)
    {
        for(tt = t; tt>0; tt=sleep(tt));
        if(SIGUSR1 == last_signal)
            printf("Success [%d]\n", getpid());
        else
            printf("Failed [%d}\n", getpid());
    }
    printf("[%d] Terminates\n", getpid());
}

void parent_work(int k, int p, int r)
{
    struct timespec tt, tk = {k, 0};
    struct timespec tp = {p, 0};
    sethandler(sig_handler, SIGALRM);
    //signal(SIGALRM, sig_handler);
    alarm(r*10);
    while(last_signal != SIGALRM)
    {
        for(tt = tk; nanosleep(&tt, &tt);)
            if(EINTR!=errno)    perror("nanosleep");

        //nanosleep(&tk, NULL);
        if(kill(0, SIGUSR1) < 0) ERR("kill");
        //printf("Sent SIGUSR1 signal\n");
        nanosleep(&tp, NULL);
        if(kill(0, SIGUSR2) < 0) ERR("kill");
        //printf("Sent SIGUSR2 signal\n");
    }
    printf("[PARENT] Terminates\n");
}

void create_children(int n, int r)
{
    while( n-- > 0)
    {
        switch(fork())
        {
            case 0:
                sethandler(sig_handler, SIGUSR1);
                sethandler(sig_handler, SIGUSR2);
                //signal(SIGUSR1, sig_handler);
                //signal(SIGUSR2, sig_handler);
                child_work(r);
                exit(EXIT_SUCCESS);
            case -1:
                perror("Fork");
                exit(EXIT_FAILURE);
        }
    }
}

void usage(void)
{
    fprintf(stderr, "USAGE: signals n k p r\n");
    exit(EXIT_FAILURE);
}

int main(int argc, const char** argv)
{
    int n, k, p, r;
    if(argc != 5)   usage();
    n = atoi(argv[1]);
    k = atoi(argv[2]);
    p = atoi(argv[3]);
    r = atoi(argv[4]);
    if(n <= 0 || k <= 0 || p <= 0 || r <= 0)    usage();

    signal(SIGCHLD, sigchld_handler);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
    create_children(n, r);
    parent_work(k, p, r);
    while(wait(NULL) > 0);
    return EXIT_SUCCESS;
}