#include <stdio.h>
#include <stdlib.h>
//#include <string.h>

#define MAX_LINE 20

#define ERR(source) (perror(source), \
                    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                    exit(EXIT_FAILURE))

extern char **environ;

int main(int argc, const char** argv)
{
    char* times = getenv("TIMES");
    if(times)   ERR("TIMES == NULL");

    long int times_num = strtol(times, NULL, 10);
    if(times_num == 0L) ERR("TIMES - WRONG FORMAT OR ZERO\n");

    char name[MAX_LINE+2];
    while(fgets(name, MAX_LINE+2, stdin) != NULL)
        for(int i = 0; i < times_num; i++)
            printf("Hello %s", name);

    setenv("RESULT", "Done", 1);

    return EXIT_SUCCESS;
}