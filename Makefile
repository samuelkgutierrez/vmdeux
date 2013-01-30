SHELL  = /bin/sh
TARGET = bitmachine
CFLAGS = -Wall -g -O0
CFLAGS = -Wall -DNDEBUG -O3

.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: clean all

all: ${TARGET}

clean:
	/bin/rm -f ${TARGET}
