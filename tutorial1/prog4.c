#include<stdio.h>
#include <stdlib.h>

int main(int argc, const char** argv)
{
    for(int i=0; i<argc; i++)
    {
        printf("%s ", argv[i]);
    }

    printf("\n");

    return EXIT_SUCCESS;
}