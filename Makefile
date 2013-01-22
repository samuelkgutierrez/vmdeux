SHELL  = /bin/sh
TARGET = bitmachine
CFLAGS = -Wall

.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: clean all

all: ${TARGET}

clean:
	/bin/rm -f ${TARGET}
