CC = gcc
CFLAGS = -Wall -Wno-unused-function -g $(shell pkg-config fuse --cflags) -std=gnu11
LDLIBS = $(shell pkg-config fuse --libs)

all: ext2fs ext2test

ext2fs: ext2fs.o ext2.o
ext2test: ext2test.o ext2.o

ext2fs.o: ext2fs.c ext2.h
ext2.o: ext2.c ext2.h

clean:
	-rm -rf ext2fs ext2test ext2fs.o ext2.o ext2test.o
tidy: clean
	-rm -rf *~
