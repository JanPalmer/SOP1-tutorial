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
#define ERR(source) (perror(source),\
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
                    exit(EXIT_FAILURE))

typedef struct argCell
{
    pthread_t tid;
    int IsUsed;
} argCell_t;

typedef struct argTab
{
    argCell_t* Tab; //table of argCells
    int* CellNum; //total number of cells in Tab
    int MaxTableSize;
    pthread_mutex_t* mxTab;
    pthread_mutex_t* mxCellNum;
} argTab_t;

typedef struct argOverseer
{
    argTab_t* CellTable;
    pthread_t tid;
    unsigned int time;
} argOverseer_t;

void ReadArguments(int argc, char** argv, int* _n, int* _t);
void* thread_overseer(void* arg);
void* thread_generator(void* arg);
void KillTable(argTab_t* tab);
int SigHandler(int sig, argOverseer_t* tab);
void msleep(UINT milisec);

int main(int argc, char** argv)
{
    int n, t;
    ReadArguments(argc, argv, &n, &t);
    
    argCell_t* table = malloc(sizeof(argCell_t)*2*n);
    if(table == NULL) ERR("malloc table of cells");
    pthread_mutex_t mxTab = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mxCellNum = PTHREAD_MUTEX_INITIALIZER;
    int CellNumber = 0;
    argTab_t CellTable = {
        .Tab = table,
        .CellNum = &CellNumber,
        .MaxTableSize = 2*n,
        .mxTab = &mxTab,
        .mxCellNum = &mxCellNum
    };
    argOverseer_t overseer = {
        .CellTable = &CellTable,
        .time = t
    };
    sigset_t newMask, oldMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGQUIT);
    if(pthread_sigmask(SIG_BLOCK, &newMask, &oldMask)) ERR("pthread_sigmask");

    if(pthread_create(&overseer.tid, NULL, thread_overseer, &overseer)) ERR("pthread_create");

    int signo = -1;
    while(1)
    { 
        if(sigwait(&newMask, &signo)) ERR("sigwait");
        if(SigHandler(signo, &overseer) == 1) break;
        signo = -1;
    }
    pthread_mutex_lock(&mxTab);
    for(int i=0; i<CellTable.MaxTableSize; i++)
    {
        if(CellTable.Tab[i].IsUsed == 1) 
        {
            pthread_join(CellTable.Tab[i].tid, NULL);
            printf("[%lu] Joined\n", CellTable.Tab[i].tid);
        }
    }
    pthread_mutex_unlock(&mxTab);
    pthread_join(overseer.tid, NULL);
    printf("Overseer Joined\n");

    free(table);
    return EXIT_SUCCESS;
}

void ReadArguments(int argc, char** argv, int *_n, int *_t)
{
    if(argc < 3) ERR("usage");
    if(argc >= 3)
    {
        *_n = atoi(argv[1]);
        *_t = atoi(argv[2]);
        if(*_n <= 0 || *_n > 20 || *_t < 100 || *_t > 5000)
        {
            printf("Invalid value for 'n' or 't'");
            exit(EXIT_FAILURE);
        }
    }
}

void* thread_overseer(void* arg)
{
    argOverseer_t* args = (argOverseer_t*) arg;
    UINT time = args->time;
    int work = 0;
    while(1)
    {
        msleep(time);
        pthread_mutex_lock(args->CellTable->mxCellNum);
        if(*args->CellTable->CellNum * 2 > args->CellTable->MaxTableSize) work = 1;
        pthread_mutex_unlock(args->CellTable->mxCellNum);

        if(work == 1)
        {
            pthread_mutex_lock(args->CellTable->mxCellNum);
            int CellsToEradicate = *args->CellTable->CellNum/2;
            printf("Too many threads.\nThreads to cancel: %d\n", CellsToEradicate);
            pthread_mutex_lock(args->CellTable->mxTab);
            for(int i=0; CellsToEradicate > 0 && i<args->CellTable->MaxTableSize; i++)
            {
                if(args->CellTable->Tab[i].IsUsed == 1)
                {
                    pthread_cancel(args->CellTable->Tab[i].tid);
                    pthread_join(args->CellTable->Tab[i].tid, NULL);
                    printf("[%lu] Cancelled and Joined\n", args->CellTable->Tab[i].tid);
                    args->CellTable->Tab[i].IsUsed = 0;
                    CellsToEradicate--;
                    (*args->CellTable->CellNum)--;
                }
            }
            work = 0;
            pthread_mutex_unlock(args->CellTable->mxTab);
            pthread_mutex_unlock(args->CellTable->mxCellNum);
        }

    }

    return NULL;
}

void* thread_generator(void* arg)
{
    UINT seed = pthread_self();
    UINT time;
    while(1)
    {
        time = rand_r(&seed)%901 + 100;
        msleep(time);
        printf("[%lu] generator\n", pthread_self());
    }
    return NULL;
}

int FindEmpty(argTab_t* tab)
{
    int i=0;
    for(; i<tab->MaxTableSize; i++)
    {
        if(tab->Tab[i].IsUsed == 0)
            return i;
    }
    return -1;
}

void KillTable(argTab_t* tab)
{
    for(int i=0; i<tab->MaxTableSize; i++)
    {
        if(tab->Tab[i].IsUsed == 1)
        {
            pthread_cancel(tab->Tab[i].tid);
            printf("Cancelling [%lu]\n", tab->Tab[i].tid);
        }
    }
    return;
}

int SigHandler(int sig, argOverseer_t* tab)
{
    switch(sig)
        {
            case SIGINT:
            {
                pthread_mutex_lock(tab->CellTable->mxTab);
                int it = FindEmpty(tab->CellTable);
                if(it == -1) return 0;

                if(pthread_create(&tab->CellTable->Tab[it].tid, NULL, thread_generator, tab)) ERR("pthread_create_generator");
                tab->CellTable->Tab[it].IsUsed = 1;
                pthread_mutex_unlock(tab->CellTable->mxTab);
                pthread_mutex_lock(tab->CellTable->mxCellNum);
                tab->CellTable->CellNum++;
                pthread_mutex_unlock(tab->CellTable->mxCellNum);
                break;
            }
            case SIGQUIT:
            {
                pthread_mutex_lock(tab->CellTable->mxTab);
                KillTable(tab->CellTable);
                pthread_mutex_unlock(tab->CellTable->mxTab);
                pthread_cancel(tab->tid);
                printf("Cancelling Overseer\n");
                return 1;
            }
            default:
                break;
        }

    return 0;
}

void msleep(UINT milisec) {
    time_t sec= (int)(milisec/1000);
    milisec = milisec - (sec*1000);
    struct timespec req= {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if(nanosleep(&req,&req)) ERR("nanosleep");
}