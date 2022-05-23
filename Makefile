CC = gcc
CFLAGS = -std=gnu99 -Wall -fsanitize=address,undefined -g
LDFLAGS = -fsanitize=address,undefined
LDLIBS = -lpthread -lm -lrt

prog: prog.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o prog prog.c
.PHONY: clean all