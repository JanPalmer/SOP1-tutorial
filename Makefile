CC = gcc
CFLAGS = -std=gnu99 -Wall -fsanitize=thread,undefined
LDFLAGS = -fsanitize=thread,undefined
LDLIBS = -lm -lrt

prog: prog.c
	$(CC) $(CFLAGS) $(LDLIBS) -o prog prog.c
.PHONY: clean all