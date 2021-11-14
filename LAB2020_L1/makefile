CC = gcc
CFLAGS = -Wall -fsanitize=undefined,address -pedantic
LDFLAGS = -fsanitize=address,undefined

prog: prog.c
	$(CC) $(CFLAGS) -o prog prog.c
.PHONY: clean all
clean:
	rm prog
