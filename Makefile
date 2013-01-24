SHELL  = /bin/sh
TARGET = bitmachine
#CFLAGS = -Wall -DNDEBUG
CFLAGS = -Wall -g -O0

.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: clean all

all: ${TARGET}

clean:
	/bin/rm -f ${TARGET}
