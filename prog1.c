#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <aio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#define AIOBLOCKS 4
#define BUFFERS AIOBLOCKS

void error(char *);
void usage(char *);
void processargs(int, char**, char**, char*, int*);
void fillaiostructs(struct aiocb*, char**, int*, int);
off_t getfilelength(int);
void readdata(struct aiocb*, off_t);
void writedata(struct aiocb*, off_t);
void suspend(struct aiocb*);
void cleanup(char**, int*);
void processfile(struct aiocb*, char**, int);
int processbuffer(char**, int);
int processresults(int a, int b, int c);

void error(char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}
void usage(char *progname){
    fprintf(stderr, "%s workfile blocksize\n", progname);
    fprintf(stderr, "workfile - path to the file to work on\n");
    fprintf(stderr, "n - number of blocks\n");
    fprintf(stderr, "k - number of iterations\n");
    exit(EXIT_FAILURE);
}
void processargs(int argc, char** argv, char** filelist, char* input, int* blocksize){
    if(argc != 6) usage(argv[0]);
    for(int i=0; i<3; i++) filelist[i] = argv[i+1]; // process filenames
    input = argv[4]; // input filename
    *blocksize = atoi(argv[5]); // blocksize
    if(*blocksize <= 0) error("Blocksize too small");

    printf("%s, %s, %s, %s, %d\n", filelist[0], filelist[1], filelist[2], input, *blocksize);

    return;
}
void fillaiostructs(struct aiocb *aiocbs, char** buffer, int* fd, int NumOfButes){
    int i;
    for(i = 0; i < AIOBLOCKS; i++){
        memset(&aiocbs[i], 0, sizeof(struct aiocb));
        aiocbs[i].aio_fildes = fd[i];
        aiocbs[i].aio_offset = 0;
        aiocbs[i].aio_nbytes = NumOfButes;
        aiocbs[i].aio_buf = (void*) buffer[i];
        aiocbs[i].aio_sigevent.sigev_notify = SIGEV_NONE;
    }
}
off_t getfilelength(int fd){
    struct stat buf;
    if(fstat(fd, &buf) == -1) error("Cannot fstat file");
    return buf.st_size;
}
void readdata(struct aiocb* aiocbp, off_t offset){
    aiocbp->aio_offset = offset;
    if(aio_read(aiocbp) == -1) error("Cannot read");
}
void writedata(struct aiocb* aiocbp, off_t offset){
    aiocbp->aio_offset = offset;
    if(aio_write(aiocbp) == -1) error("Cannot write");
}
void suspend(struct aiocb* aiocbs){
    struct aiocb *aiolist[1];
    aiolist[0] = aiocbs;
    while(aio_suspend((const struct aiocb *const *) aiolist, 1, NULL) == -1){
        if(errno == EINTR) continue;
        error("Suspend error");
    }
    if(aio_error(aiocbs) != 0) error("Suspend error");
    if(aio_return(aiocbs) == -1) error("Return error");
}
void cleanup(char** buffers, int* fd){
    int i;
    for(i = 0; i < AIOBLOCKS; i++)
        if(aio_cancel(fd[i], NULL) == -1) error("Cannot cancel async I/O");

    for(i = 0; i < BUFFERS; i++){
        free(buffers[i]);
    }
    for(i = 0; i < AIOBLOCKS; i++)
        if(TEMP_FAILURE_RETRY(fsync(fd[i])) == -1) error("Error running fsync");
}
void processfile(struct aiocb* aiocbs, char** buffer, int BufSize){
    srand(time(NULL));
    int fileBlockNumber = getfilelength(aiocbs->aio_fildes)/BufSize;
    int it = 0, res, dest;
    int IsUsed[AIOBLOCKS] = {0, 0, 0, 0};
    while(it < fileBlockNumber){
        readdata(&aiocbs[0], it*BufSize);
        suspend(&aiocbs[0]);
        res = processbuffer(buffer, BufSize);
        if(res == 0)
            dest = rand()%3 + 1;
        else
            dest = res;

        if(IsUsed[dest] > 0) suspend(&aiocbs[dest]);

        strcpy(buffer[dest], buffer[0]);
        writedata(&aiocbs[dest], it*BufSize);
        IsUsed[dest] = 1;
        it++;
    }

    // initial suspend, might be redundant due to cleanup()
    for(it = 0; it < AIOBLOCKS; it++) suspend(&aiocbs[it]);
}
int processbuffer(char** buffer, int BufSize){
    int number = 0, letter = 0, other = 0;
    for(int bi = 0; bi < BufSize; bi++){
        // check if is a number
        if(buffer[0][bi] >= '0' && buffer[0][bi] <= '9'){
            number++;
            continue;;
        }

        if((buffer[0][bi] >= 'a' && buffer[0][bi] <= 'z') || (buffer[0][bi] >= 'A' && buffer[0][bi] <= 'Z')){
            letter++;
            continue;;
        }

        other++;
    }
    return processresults(number, letter, other);
}
int processresults(int a, int b, int c){
    // 0 - equal, 1 - a, 2 - b, 3 - c
    if(a == b || a == c || b == c) return 0;

    if(a > b){
        if(a > c)
            return 1;
        else
            return 3;
    }
    
    if(b > c)
        return 2;
    else
        return 3;
}

int main(int argc, char** argv){
    char *buffer[BUFFERS];
    char *filenames[AIOBLOCKS], *inputfile;
    int fd[AIOBLOCKS], blocksize; // fd[0] - input, fd[1-3] - output
    struct aiocb aiocbs[AIOBLOCKS];
    processargs(argc, argv, filenames, inputfile, &blocksize);
    inputfile = argv[4];

    if((fd[0] = TEMP_FAILURE_RETRY(open(inputfile, O_RDONLY))) == -1) error("Cannot open input file");
    for(int i=1; i < AIOBLOCKS; i++)
        if((fd[i] = TEMP_FAILURE_RETRY(open(filenames[i-1], O_RDWR | O_TRUNC | O_CREAT, 0666))) == -1)
            error("Cannot open output file");

    for(int i=0; i < BUFFERS; i++) buffer[i] = (char*) calloc(blocksize, sizeof(char));

    fillaiostructs(aiocbs, buffer, fd, blocksize);
    processfile(aiocbs, buffer, blocksize);
    cleanup(buffer, fd);

    return EXIT_SUCCESS;
}