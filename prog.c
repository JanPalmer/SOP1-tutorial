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

typedef struct cell
{
    int Val;
    pthread_mutex_t mx;
} cell_t;

typedef struct argsWork
{
    int ThrNum;
    pthread_t tid;
    int W;
    int N;
    cell_t *tab;
} argsWork_t;

struct mut
{
    pthread_mutex_t *mx1;
    pthread_mutex_t *mx2;
};

void ReadArguments(int argc, char** argv, int *_N, int *_T, int *_W);
void* thread_work(void* arg);
void cellTableInit(cell_t *_t, int len);
void UnlockMutex(void* arg);
void swap(int* a, int* b);
void msleep(UINT milisec);

int main(int argc, char** argv)
{
    int N, T, W;
    ReadArguments(argc, argv, &N, &T, &W);
    argsWork_t threads[T];
    cell_t table[N];
    for(int i=0; i<N; i++) 
    {
        table[i].Val = N-1-i;
        pthread_mutex_init(&table[i].mx, NULL);
    }

    for(int i=0; i<T; i++)
    {
        if(pthread_create(&threads[i].tid, NULL, thread_work, &threads[i])) ERR("pthread_create");
        threads[i].ThrNum = i;
        threads[i].W = W;
        threads[i].N = N;
        threads[i].tab = table;
    }

    msleep(5000);
    for(int i=0; i<T; i++)
    {
        pthread_cancel(threads[i].tid);
    }
    //printf("Threads cancelled\n");
    for(int i=0; i<T; i++)
    {
        //printf("[%lu] Joining\n", threads[i].tid);
        if(pthread_join(threads[i].tid, NULL)) ERR("pthread_join");
        //printf("[%lu] Joined\n", threads[i].tid);
    }

    for(int i=0; i<N; i++) 
    {
        printf("%d ", table[i].Val);
        pthread_mutex_destroy(&table[i].mx);
    }
    printf("\n");

    return EXIT_SUCCESS;
}

void ReadArguments(int argc, char** argv, int *_N, int *_T, int *_W)
{
    if(argc < 4) ERR("usage");

    *_N = atoi(argv[1]);
    *_T = atoi(argv[2]);
    *_W = atoi(argv[3]);

    if(*_N < 5 || *_N > 20) ERR("Wrong N argument");
    if(*_T < 2 || *_T > 40) ERR("Wrong T argument");
    if(*_W < 10 || *_W > 1000) ERR("Wrong W argument");
    return;
}

void* thread_work(void* arg)
{
    argsWork_t* args = (argsWork_t*) arg;
    UINT seed = pthread_self();
    int time;
    int index;
    while(1)
    {
        time = rand_r(&seed)%(args->W - 5 + 1) + 5;
        msleep(time);
        index = rand_r(&seed)%args->N;
        struct mut* temp = malloc(sizeof(struct mut));
        if(temp == NULL) ERR("malloc");
        if(index == args->N-1)
        {
            temp->mx1 = &args->tab[0].mx;
            temp->mx2 = &args->tab[index].mx;
            pthread_cleanup_push(UnlockMutex, temp);
            pthread_mutex_lock(&args->tab[0].mx);
            msleep(200);
            pthread_mutex_lock(&args->tab[index].mx);
            if(args->tab[index].Val < args->tab[0].Val) swap(&args->tab[index].Val, &args->tab[0].Val);
            pthread_cleanup_pop(1);

        }
        else
        {
            temp->mx1 = &args->tab[index].mx;
            temp->mx2 = &args->tab[index+1].mx;
            pthread_cleanup_push(UnlockMutex, temp);
            pthread_mutex_lock(&args->tab[index].mx);
            msleep(200);
            pthread_mutex_lock(&args->tab[index+1].mx);
            if(args->tab[index].Val > args->tab[index+1].Val) swap(&args->tab[index].Val, &args->tab[index+1].Val);
            pthread_cleanup_pop(1);
        } 
    }
    return NULL;
}

void UnlockMutex(void* arg)
{
    struct mut* args = (struct mut*) arg;
    pthread_mutex_unlock(args->mx1);
    pthread_mutex_unlock(args->mx2);
    free(args);
    return;
}

void cellTableInit(cell_t *_t, int len)
{
    for(int i=0; i<len; i++) 
    {
        _t[i].Val = len-1-i;
        pthread_mutex_init(&_t[i].mx, NULL);
    }
}

void swap(int* a, int* b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
    return;
}


void msleep(UINT milisec) {
    time_t sec= (int)(milisec/1000);
    milisec = milisec - (sec*1000);
    struct timespec req= {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if(nanosleep(&req,&req)) ERR("nanosleep");
}