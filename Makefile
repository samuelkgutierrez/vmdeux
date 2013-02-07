SHELL  = /bin/sh
TARGET = vmdeux
CFLAGS = -Wall -g -O0
CFLAGS = -Wall -DNDEBUG -O3
# gprof
CFLAGS = -Wall -g -pg

.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: clean all

all: ${TARGET}

${TARGET}: redblack.o

redblack.o: redblack.h redblack.c

test-rb: redblack.o

clean:
	/bin/rm -f ${TARGET} *.o
