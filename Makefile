all:send receive

send: 
	gcc -Wall -o send send.c

receive: 
	gcc -Wall -o receive receive.c
