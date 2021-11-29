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

void child_work(int time, int r)
{
    struct timespec t = {0, time*10000};
    int sig_count = 0;

    while(1)
    {
        // for(tt = t; nanosleep(&tt, &tt);)
        //     if(EINTR!=errno)    perror("nanosleep");

        for(int i=0; i<r; i++)
        {
            nanosleep(&t, 0);
            if(kill(getppid(), SIGUSR1)) ERR("kill");
        }

        nanosleep(&t, 0);
        if(kill(getppid(), SIGUSR2)) ERR("kill");
        sig_count++;
        printf("[%d] sent %d SIGUSR2\n", getpid(), sig_count);
    }
    
    printf("[%d] Terminates\n", getpid());
}

void parent_work()
{
    int count = 0;
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    //sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    while(1)
    {
        last_signal = 0;
        while(last_signal != SIGUSR2)
            sigsuspend(&old_mask);

        count++;
        printf("[%d] received %d SIGUSR2\n", getpid(), count);
    }

    printf("[PARENT] Terminates\n");
}

void usage(void)
{
    fprintf(stderr, "USAGE: signals n k p r\n");
    exit(EXIT_FAILURE);
}

int main(int argc, const char** argv)
{
    int n, t;
    if(argc != 3)   usage();
    n = atoi(argv[2]);
    t = atoi(argv[1]);

    signal(SIGCHLD, sigchld_handler);
    signal(SIGUSR1, sig_handler);
    sethandler(sig_handler, SIGUSR2);
    switch(fork())
    {
        case 0:
            child_work(t, n);
            exit(0);
        default:
            parent_work();
            break;
    }


    
    while(wait(NULL) > 0);
    return EXIT_SUCCESS;
}