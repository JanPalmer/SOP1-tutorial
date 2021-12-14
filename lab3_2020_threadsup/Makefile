CC = gcc
CFLAGS = -std=gnu99 -Wall -fsanitize=thread,undefined
LDFLAGS = -fsanitize=thread,undefined
LDLIBS = -lpthread -lm

prog: prog.c
	$(CC) $(CFLAGS) -o prog prog.c