CC = gcc
CFLAGS = -Wall
INCLUDES = -I./struct.h

all: send receive
send: send.c
	$(CC) -o $@ $(CFLAGS) $^  
receive: receive.c
	$(CC) -o $@ $(CFLAGS) $^  

clean:
	rm -f send receive
