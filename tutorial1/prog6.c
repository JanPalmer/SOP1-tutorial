#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

void usage(char* pname)
{
    fprintf(stderr, "USAGE:%s ([-t x] -n Name\n", pname);
    exit(EXIT_FAILURE);
}

int main(int argc, const char** argv)
{
    int x = 1;
    int c;

    while((c = getopt(argc, argv, "t:n:")) != -1)
    {
        switch(c)
        {
            case 't':
                x = atoi(optarg);
                break;
            case 'n':
                for(int i=0; i<x; i++)  printf("Hello %s\n", optarg);
                break;
            case '?':
            default:
                usage((char*)argv[0]);
        }
    }

    if(argc > optind)   usage((char*)argv[0]);


    return EXIT_SUCCESS;
}