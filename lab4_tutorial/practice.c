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

// program wczytuje plik
// jeden aio zczytuje pierwsza polowe pliku i dopisuje ja na koniec
// drugi aio zczytuje druga polowe i wypisuje na stdout

void error(char *);
void usage(char *);
void fillaiostructs(struct aiocb*, char**, int, int);
off_t getfilelength(int);
void readdata(struct aiocb*, off_t);
void writedata(struct aiocb*, off_t);
void suspend(struct aiocb* aiocbs);
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
void fillaiostructs(struct aiocb *aiocbs, char** buffer, int fd, int NumOfButes){
    int i;
    for(i = 0; i < AIOBLOCKS; i++){
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

    for(i = 0; i < 2; i++){
        free(buffers[i]);
    }
    if(TEMP_FAILURE_RETRY(fsync(fd)) == -1) error("Error running fsync");
}
void processfile(struct aiocb* aiocbs, char** buffer, int NumOfBytes){
    //czytanie
    readdata(&aiocbs[0], 0);
    readdata(&aiocbs[1], NumOfBytes);
    //dopisanie
    suspend(&aiocbs[0]);
    writedata(&aiocbs[2], 0);
    //wypisanie
    suspend(&aiocbs[1]);
    write(STDOUT_FILENO, buffer[1], NumOfBytes);
    suspend(&aiocbs[2]);
}

int main(int argc, char** argv){
    char *filename, *buffer[2];
    int fd;
    struct aiocb aiocbs[AIOBLOCKS];

    // program pryjmuje 1 argument - sciezke do pliku
    if(argc != 2) usage(argv[0]);
    filename = argv[1];
    if((fd = TEMP_FAILURE_RETRY(open(filename, O_RDWR | O_APPEND))) == -1) error("Cannot open file");
    int halfpoint = (getfilelength(fd))/2;
    if(halfpoint > 0)
    {
        for(int i=0; i < 2; i++) buffer[i] = (char*) calloc(halfpoint, sizeof(char));

        fillaiostructs(aiocbs, buffer, fd, halfpoint);
        aiocbs[2].aio_buf = aiocbs[0].aio_buf;
        processfile(aiocbs, buffer, halfpoint);
        cleanup(buffer, fd);
    }

    return EXIT_SUCCESS;
}