#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define ERR(source) (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
                    perror(source), kill(0, SIGKILL),\
                    exit(EXIT_FAILURE))

void sig_handler(int signum)
{
    //printf("%d received\n", signum);
}

ssize_t bulk_write(int fd, char *buf, size_t count){
	ssize_t c;
	ssize_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

void child_work(int n)
{
    srand(getpid());
    int s = 10 + rand()%(100 + 1);
    int out;
    char name[100];
    if(sprintf(name, "%d.TXT", getpid()) < 0) ERR("sprintf");

    //printf("[%d] n: %d, s: %d\n", getpid(), n, s);

    int count = 0, def;
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    //sigprocmask(SIG_BLOCK, &mask, &old_mask);

    struct timespec t = {1, 0}, tt;

    char* buf = malloc(s * 1024);
    memset(buf, n, s*1024);

    if((out=open(name, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0777)) < 0)
        ERR("open");

    //for(tt = t; nanosleep(&tt, &tt);)
    for(; ;)
    {
        if(EINTR!=errno)    perror("nanosleep");

        //sigsuspend(&old_mask);
        sigwait(&mask, &def);
        count++;
        if((bulk_write(out,buf, s*1024))<0) ERR("write");
        //printf("[%d] count: %d\n", getpid(), count);
    }

    if(close(out)) ERR("close");
    free(buf);
    return;     
}

void parent_work()
{
    signal(SIGUSR1, SIG_IGN);

    struct timespec t = {0, 10};
    int time = 1000/10;

    for(int i = 0; i< time; i++)
    {
        if(nanosleep(&t, 0) < 0) ERR("nanosleep");
        kill(-0, SIGUSR1);
    }

    return;
}

int main(int argc, const char** argv)
{
    if(argc <= 1)   return EXIT_FAILURE;

    signal(SIGUSR1, sig_handler);

    for(int i=0; i < argc-1; i++)
    {
        switch(fork())
        {
            case -1:
                ERR("fork");
                break;
            case 0:
                child_work(atoi(argv[i+1]));
                exit(EXIT_SUCCESS);
            default:
            parent_work();
            while(wait(NULL) > 0);
        }
    }

    return EXIT_SUCCESS;
}