#include <bits/types/sigset_t.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#define UINT unsigned int
#define DEFAULT_THREAD_COUNT 10
#define ERR(source) (perror(source),\
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
                    exit(EXIT_FAILURE))

typedef struct argsStruct
{
    pthread_t tid;
    int M;
    int* L;
    int* flagSIGINT;
    int* thrCounter;
    pthread_mutex_t *mxL;
    pthread_mutex_t *mxFlag;
} argsStruct_t;

struct argsSigHandler
{
    pthread_t tid;
    sigset_t *mask;
    int* flagSIGINT;
    pthread_mutex_t *mxFlag;
};

void ReadArguments(int argc, char** argv, int* num);
void* thread_work(void* arg);
void* SigHandler(void* arg);
void msleep(UINT milisec);

int main(int argc, char** argv)
{
    int n = DEFAULT_THREAD_COUNT;
    ReadArguments(argc, argv, &n);

    argsStruct_t args[n];
    int L = 1;
    int flagSIGINT = 0;
    int thrCounter = 0;
    pthread_mutex_t mx;
    pthread_mutex_t mxFlag = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&mx, NULL);
    args[0].L = &L;
    args[0].flagSIGINT = &flagSIGINT;
    args[0].thrCounter = &thrCounter;
    args[0].mxL = &mx;
    args[0].mxFlag = &mxFlag;
    for(int i=1; i<n; i++) args[i] = args[0];

    sigset_t newMask, oldMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    if(pthread_sigmask(SIG_BLOCK, &newMask, &oldMask)) ERR("pthread_sigmask");
    struct argsSigHandler argsSig;
    argsSig.mask = &newMask;
    argsSig.mxFlag = &mxFlag;
    argsSig.flagSIGINT = &flagSIGINT;

    if(pthread_create(&argsSig.tid, NULL, SigHandler, &argsSig)) ERR("pthread_create");
    //printf("[%lu] signal thread created\n", argsSig.tid);

    for(int i=0; i<n; i++)
    {
        if(pthread_create(&args[i].tid, NULL, thread_work, &args[i])) ERR("pthread_create");
        //printf("[%lu] worker thread created\n", args[i].tid);
    }

    while(1)
    {
        msleep(100);
        
        //check if SIGINT has been sent
        pthread_mutex_lock(&mxFlag);
        if(flagSIGINT == 1)   break;
        pthread_mutex_unlock(&mxFlag);

        pthread_mutex_lock(&mx);
        if(thrCounter >= n)
        {
            L++;
            thrCounter = 0;
        }
        pthread_mutex_unlock(&mx);
    }

    for(int i=0; i<n; i++)
    {
        pthread_join(args[i].tid, NULL);
        //printf("[%lu] worker thread joined\n", args[i].tid);
    }
    pthread_join(argsSig.tid, NULL);
    //printf("[%lu] signal thread joined\n", argsSig.tid);

    return EXIT_SUCCESS;
}

void ReadArguments(int argc, char** argv, int* num)
{
    char c;
    while((c = getopt(argc, (char* const*)argv, "n:")) != -1)
    {
        switch(c)
        {
            case 'n':
                *num = atoi(optarg);
                if(*num <= 0) ERR("Wrong thread count number");
                break;
            case '?':
            default:
                ERR(argv[0]);
        }
    }
    return;
}

void* thread_work(void* arg)
{
    argsStruct_t* tab = (argsStruct_t*) arg;
    unsigned int seed = (unsigned int) tab->tid;
    tab->M = rand_r(&seed)%99 + 2;
    int lastL;
    //printf("%d\n", tab->M);
    while(1)
    {
        //check if SIGINT has been sent
        pthread_mutex_lock(tab->mxFlag);
        if(*tab->flagSIGINT == 1)
        {
            pthread_mutex_unlock(tab->mxFlag);
            break;
        }
        pthread_mutex_unlock(tab->mxFlag);

        //check division
        pthread_mutex_lock(tab->mxL);
        if(*tab->L != lastL)
        {
            if(*tab->L%tab->M == 0)  printf("[%d] %d podzielne przez %d\n", *tab->thrCounter, *tab->L, tab->M);
            (*tab->thrCounter) += 1;
            lastL = *tab->L;
        }
        pthread_mutex_unlock(tab->mxL);
    }

    //printf("[%lu] Worker thread closing\n", pthread_self());
    return NULL;
}

void* SigHandler(void* arg)
{
    struct argsSigHandler *args = arg;
    int signo;
    for(;;)
    {
        if(sigwait(args->mask, &signo)) ERR("sigwait");
        switch(signo)
        {
            case SIGINT:
                pthread_mutex_lock(args->mxFlag);
                *args->flagSIGINT = 1;
                //printf("SIGINT received\n");
                pthread_mutex_unlock(args->mxFlag);
                //printf("[%lu] Signal thread closing\n", pthread_self());
                return NULL;
            default:
                return NULL;
        }
    }

    return NULL;
}

void msleep(UINT milisec) {
    time_t sec= (int)(milisec/1000);
    milisec = milisec - (sec*1000);
    struct timespec req= {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if(nanosleep(&req,&req)) ERR("nanosleep");
}