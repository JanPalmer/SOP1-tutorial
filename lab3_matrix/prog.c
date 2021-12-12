#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define THREAD_COUNT 3
#define DEFAULT_WIDTH 5
#define DEFAULT_HEIGHT 5
#define NEXT_DOUBLE(seedptr) ((double) rand_r(seedptr) / (double) RAND_MAX)
#define ERR(source) (perror(source),\
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
                    exit(EXIT_FAILURE))

// 3 watki - jeden od kolorowania kolumn, drugi od wierszy, trzeci od sygnalow
// omutexowana zmienna
// otrzymujac sygnal trzeci watek losuje wartosc zmiennej losowej, po czym
// rozkazuje odpowiedniemu watkowi zwiekszenie wartosci w kolumnie lub wierszu
// Sygnal SIGINT Ctrl-C - zwiekszenie wiersza
// Sygnal SIGQUIT Ctrl-\ - zwiekszenie kolumny
// main czeka na input, jesli input to exit, konczy i wychodzi
// 2 mutexy - jeden pod zmienna losowa w trzecim watku, drugi pod matryce

typedef struct argsThread
{
    pthread_t tid;
} argsThread_t;

typedef struct argsMutex
{
    char** matrix;
    int heigth;
    int width;
    int* colNum;
    int* rowNum;
    pthread_mutex_t *pmxNumber;
    pthread_mutex_t *pmxMatrix;
    sigset_t *mask;
    argsThread_t *tab;
} argsMutex_t;

char** createMatrix(int h, int w);
void ReadArguments(int argc, char** argv, int *_heigth, int *_width);
void InitMatrix(char** _matrix, int _heigth, int _width);
void FreeMatrix(char** _matrix, int _heigth, int _width);
void PrintMatrix(char** _matrix, int _heigth, int _width);
void* SigHandler(void* arg);
void* ColumnHandler(void* arg);
void* RowHandler(void* arg);

int main(int argc, char** argv)
{
    int Width, Heigth;
    ReadArguments(argc, argv, &Heigth, &Width);
    char **screen = createMatrix(Heigth, Width);
    InitMatrix(screen, Heigth, Width);
    PrintMatrix(screen, Heigth, Width);

    argsThread_t tids[3];

    pthread_mutex_t mxNumber;
    pthread_mutex_t mxMatrix;
    if(pthread_mutex_init(&mxNumber, NULL) != 0) ERR("mutex init");
    if(pthread_mutex_init(&mxMatrix, NULL) != 0) ERR("mutex init");
    int rowRandomNumber, columnRandomNumber;
    argsMutex_t mxStruct = {
        .matrix = screen,
        .pmxNumber = &mxNumber,
        .pmxMatrix = &mxMatrix,
        .heigth = Heigth,
        .width = Width,
        .colNum = &rowRandomNumber,
        .rowNum = &columnRandomNumber,
        .tab = tids
    };

    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGQUIT);
    sigaddset(&newMask, SIGUSR1);
    if(pthread_sigmask(SIG_BLOCK, &newMask, &oldMask)) ERR("SIG_BLOCK error");
    mxStruct.mask = &newMask;
    argsMutex_t mxTab[THREAD_COUNT];
    for(int i=0; i < THREAD_COUNT; i++) mxTab[i] = mxStruct;

    if(pthread_create(&tids[2].tid, NULL, SigHandler, (void*)&mxTab[2])) ERR("Could not create signal thread");
    if(pthread_create(&tids[0].tid, NULL, ColumnHandler, (void*)&mxTab[0])) ERR("Could not create column thread");
    if(pthread_create(&tids[1].tid, NULL, RowHandler, (void*)&mxTab[1])) ERR("Could not create row thread");

    while(getchar() != 'o');

    for(int i=0; i<THREAD_COUNT; i++)
    {
        pthread_kill(tids[i].tid, SIGUSR1);
        if(pthread_join(tids[i].tid, NULL) != 0) ERR("pthread join");
    }

    pthread_mutex_destroy(&mxNumber);
    pthread_mutex_destroy(&mxMatrix);
    FreeMatrix(screen, Heigth, Width);
    exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char** argv, int *_heigth, int *_width)
{
    *_heigth = DEFAULT_HEIGHT;
    *_width = DEFAULT_WIDTH;
    if(argc >= 2)
    {
        *_heigth = atoi(argv[1]);
        if(*_heigth <= 0)
        {
            printf("Invalid value for 'ballsCount'");
            exit(EXIT_FAILURE);
        }
    }
    if(argc >= 3)
    {
        *_width = atoi(argv[2]);
        if(*_width <= 0)
        {
            printf("Invalid value for 'throwersCount'");
            exit(EXIT_FAILURE);
        }
    }
}

char** createMatrix(int h, int w)
{
    char **tmp;
    tmp = (char**)malloc(sizeof(char*)*w);
    for(int i=0; i<w; i++)
    {
        tmp[i] = (char*)malloc(sizeof(char)*h);
    }
    return tmp;
}

void InitMatrix(char** _matrix, int _heigth, int _width)
{
    for(int y = 0; y<_heigth; y++)
    {
        for(int x = 0; x < _width; x++)
        {
            _matrix[x][y] = '0';
        }
    }
}

void FreeMatrix(char** _matrix, int _heigth, int _width)
{
    for(int i=0; i<_width; i++)
    {
        free(_matrix[i]);
    }
    free(_matrix);
}

void PrintMatrix(char** _matrix, int _heigth, int _width)
{
    for(int y = 0; y<_heigth; y++)
    {
        for(int x = 0; x < _width; x++)
        {
            printf("%c", _matrix[x][y]);
        }
        printf("\n");
    }
}

void* SigHandler(void* arg)
{
    argsMutex_t *args = arg;
    int signo;
    srand(time(NULL));
    for(;;)
    {
        if(sigwait(args->mask, &signo)) ERR("sigwait");
        switch(signo)
        {
            case SIGINT:
                pthread_mutex_lock(args->pmxNumber);
                *(args->colNum) = rand()%args->heigth;
                *(args->rowNum) = rand()%args->width;
                pthread_mutex_unlock(args->pmxNumber);
                pthread_kill(args->tab[1].tid, SIGINT);
                break;
            case SIGQUIT:
                pthread_mutex_lock(args->pmxNumber);
                *(args->colNum) = rand()%args->heigth;
                *(args->rowNum) = rand()%args->width;
                pthread_mutex_unlock(args->pmxNumber);
                pthread_kill(args->tab[0].tid, SIGQUIT);
                break;
            default:
                return NULL;
        }
    }

    return NULL;
}

void* ColumnHandler(void* arg)
{
    argsMutex_t *args = arg;
    int signo;
    for(;;)
    {
        if(sigwait(args->mask, &signo)) ERR("sigwait");
        switch(signo)
        {
            case SIGINT:
                break;
            case SIGQUIT:
                pthread_mutex_lock(args->pmxNumber);
                pthread_mutex_lock(args->pmxMatrix);
                for(int i=0; i<args->width; i++)
                {
                    args->matrix[i][*(args->colNum)]++;
                }
                printf("\n");
                PrintMatrix(args->matrix, args->heigth, args->width);
                pthread_mutex_unlock(args->pmxMatrix);
                pthread_mutex_unlock(args->pmxNumber);
                break;
            case SIGUSR1:
                return NULL;
            default:
                return NULL;
        }
    }

    return NULL;
}

void* RowHandler(void* arg)
{
    argsMutex_t *args = arg;
    int signo;
    for(;;)
    {
        if(sigwait(args->mask, &signo)) ERR("sigwait");
        switch(signo)
        {
            case SIGINT:
                pthread_mutex_lock(args->pmxNumber);
                pthread_mutex_lock(args->pmxMatrix);
                
                for(int i=0; i<args->heigth; i++)
                {
                    args->matrix[*(args->rowNum)][i]++;
                }
                printf("\n");
                PrintMatrix(args->matrix, args->heigth, args->width);
                pthread_mutex_unlock(args->pmxMatrix);
                pthread_mutex_unlock(args->pmxNumber);
                break;
            case SIGQUIT:
                break;
            case SIGUSR1:
                return NULL;
            default:
                return NULL;
        }
    }
    return NULL;
}
