#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <aio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#define AIOBLOCKS 3
#define BUFFERS 1

void error(char *);
void usage(char *);
void processargs(int, char**, char**, char*, int*);
void fillaiostructs(struct aiocb*, char**, int, int);
off_t getfilelength(int);
void readdata(struct aiocb*, off_t);
void writedata(struct aiocb*, off_t);
void suspend(struct aiocb*);
void cleanup(char**, int);
void processfile(struct aiocb*, char**, int);

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
void fillaiostructs(struct aiocb *aiocbs, char** buffer, int fd, int NumOfButes){
    int i;
    for(i = 0; i < BUFFERS; i++){
        memset(&aiocbs[i], 0, sizeof(struct aiocb));
        aiocbs[i].aio_fildes = fd;
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
void cleanup(char** buffers, int fd){
    int i;
    if(aio_cancel(fd, NULL) == -1) error("Cannot cancel async I/O");

    for(i = 0; i < BUFFERS; i++){
        free(buffers[i]);
    }
    if(TEMP_FAILURE_RETRY(fsync(fd)) == -1) error("Error running fsync");
}
void processfile(struct aiocb* aiocbs, char** buffer, int BufSize){
    int fileBlockNumber = getfilelength(aiocbs->aio_fildes)/BufSize;
    int it = 0;
    while(it < fileBlockNumber){
        readdata(&aiocbs[0], it*BufSize);
        suspend(&aiocbs[0]);
        write(STDOUT_FILENO, buffer[0], BufSize);
        it++;
    }
}

int main(int argc, char** argv){
    char *buffer[BUFFERS];
    char *filenames[3], *inputfile;
    int fdinput, blocksize;
    struct aiocb aiocbs[AIOBLOCKS];
    processargs(argc, argv, filenames, inputfile, &blocksize);
    inputfile = argv[4];

    if((fdinput = TEMP_FAILURE_RETRY(open(inputfile, O_RDONLY))) == -1) error("Cannot open input file");
        for(int i=0; i < BUFFERS; i++) buffer[i] = (char*) calloc(blocksize, sizeof(char));

    fillaiostructs(aiocbs, buffer, fdinput, blocksize);
    processfile(aiocbs, buffer, blocksize);
    cleanup(buffer, fdinput);

    return EXIT_SUCCESS;
}