#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(char* pname)
{
    fprintf(stderr, "USAGE:%s name times>0\n", pname);
    exit(EXIT_FAILURE);
}

int main(int argc, const char** argv)
{
    if(argc != 3) usage((char*)argv[0]);

    long int n = strtol(argv[2], NULL, 10);

    if(n > 0)
    {
        for(int i=0; i<n; i++)  printf("Hello %s\n", argv[1]);
    }
    else
    {
        usage((char*)argv[0]);
    }

    return EXIT_SUCCESS;
}