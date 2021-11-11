#include <stdio.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, const char** argv)
{
    int index = 0;
    while(environ[index])
    {
        printf("%s\n", environ[index++]);
    }

    return EXIT_SUCCESS;
}