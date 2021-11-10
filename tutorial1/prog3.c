#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 20

#define ERR(source) (perror(source), \
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                    exit(EXIT_FAILURE))

int main(int argc, const char** argv)
{
    // char name[22];
    // scanf("%21s", name);
    // if(strlen(name) > 20)
    //     ERR("Name too long\n");
    // else
    //     printf("Welcome %s, welcome\n", name);

    char name[MAX_LINE+2];
    while(fgets(name, MAX_LINE+2, stdin) != NULL)
        printf("Hello %s", name);

    // while(1)
    // {
    //     fscanf(stdin, "%20s", name);
    //     printf("H-hello %s senpai UwU\n", name);
    // }

    return EXIT_SUCCESS;
}