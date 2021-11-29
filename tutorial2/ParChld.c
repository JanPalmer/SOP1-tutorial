#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

void sig_handler_parent(int signum)
{
    printf("Parent: Received a response %d\n", signum);
}

void sig_handler_child(int signum)
{
    printf("Child: Received a signal from parent\n");
    sleep(1);
    kill(getppid(), SIGUSR1);
}

int main(int argc, const char** argv)
{
    pid_t pid;
    pid = fork();

    if( 0 > pid)
    {
        printf("Fork failure\n");
        exit(EXIT_FAILURE);
    }

    if( 0 == pid) //child part
    {
        signal(SIGUSR1, sig_handler_child);
        printf("Child waiting for SIGUSR1\n");
        pause();
        //exit(EXIT_SUCCESS);
    }
    else
        if( 0 < pid) //parent instructions
        {
            signal(SIGUSR1, sig_handler_parent);
            sleep(1);
            printf("Parent sending signal\n");
            kill(pid, SIGUSR1);
            printf("Parent waiting for response\n");
            pause();
        }

    //while(wait(NULL) > 0 );

    return EXIT_SUCCESS;
}
