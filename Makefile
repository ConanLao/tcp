all: send receive
CC = gcc
CFLAGS = -Wall
LFLAGS = -pthread
INCLUDES = -I./struct.h

send: send.c
	$(CC) $(LFLAGS) -o $@ $(CFLAGS) $^  
receive: receive.c
	$(CC) $(LFLAGS) -o $@ $(CFLAGS) $^  

clean:
	rm -f send receive
