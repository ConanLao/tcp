CC = gcc
CFLAGS = -Wall
INCLUDES = -I./struct.h

all: sends receives
sends: send.c
	$(CC) -o $@ $(CFLAGS) $^  
receives: receive.c
	$(CC) -o $@ $(CFLAGS) $^  

clean:
	rm -f sends receives