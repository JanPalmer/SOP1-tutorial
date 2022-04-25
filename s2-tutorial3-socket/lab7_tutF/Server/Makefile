CC=gcc
CFLAGS=-std=gnu99 -Wall -fsanitize=address,undefined -g
LDFLAGS=-fsanitize=address,undefined
LDLIBS=-lpthread -lm -lrt

TARGET=lab7_tutF_S

SRCS := $(shell find -name '*.c') 
OBJS := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	${CC}  $(OBJS) -o $@ ${CFLAGS} ${LDFLAGS} ${LDLIBS}

%.c.o: %.c
	${CC} $< -o $@ ${CFLAGS} ${LDFLAGS} ${LDLIBS}

.PHONY: clean
clean:
	rm $(TARGET)
