SHELL  = /bin/sh
TARGET = bitmachine
CFLAGS = -Wall -g -O0
CFLAGS = -Wall -DNDEBUG

.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: clean all

all: ${TARGET}

clean:
	/bin/rm -f ${TARGET}
