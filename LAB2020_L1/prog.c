#define _XOPEN_SOURCE 500
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#define MAX_PATH 101
#define MAXFD 20

#define ERR(source) (perror(source), \
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                    exit(EXIT_FAILURE))

char file = 0;
char type = 'r';

int walk(const char* name, const struct stat *s, int type)
{

    switch (type)
    {
        case FTW_D:
        case FTW_DNR:
            if('d' == type)
            {
                if(0 == file)
                {
                    printf("%s\n", name);
                }
            }
            break;
        case FTW_F:
            if('r' == type && s->st_size >= strtol(getenv("MINSIZE"), NULL, 10))
            {
                if(errno == EINVAL) ERR("walk_strtol");
                if(0 == file)
                {
                    printf("%s\n", name);
                }
            }
            break;
        case FTW_SL:
            if('s' == type)
            {
                if(0 == file)
                {
                    printf("%s\n", name);
                }
            }
            break;
        default:
            ERR("walk_switch");   
    }
    return 0;
}

int main(int argc, const char** argv)
{
    char c;
    char* path = NULL;

    while((c = getopt(argc, (char* const*)argv, "p:t:s:f:")) != -1)
    {
        switch(c)
        {
            case 'p':
                path = optarg;
                break;
            case 't':
                type = *optarg;
                if( type != 'd' || type != 'r' || type != 's')  ERR("wrong type");
                break;
            case 's':
                if(-1 == setenv("MINSIZE", optarg, 1)) ERR("setenv");
                break;
            case 'f':
                file = 1;
                break;
            case '?':
            default:
                ERR(argv[0]);
        }
    }
    if(NULL == path)    ERR("path required");

    if(ftw(path, walk, MAXFD) == -1)    ERR("ftw");

    return EXIT_SUCCESS;
}