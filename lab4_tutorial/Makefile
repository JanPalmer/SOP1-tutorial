CC = gcc
CFLAGS = -std=gnu99 -Wall -fsanitize=thread,undefined
LDFLAGS = -fsanitize=thread,undefined
LDLIBS = -lm -lrt

prog: prog.c
	$(CC) $(CFLAGS) -o prog prog.c