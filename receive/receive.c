#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "struct.h"


int main(int argc, char *argv[])
{
	int sd = socket(PF_INET, SOCK_RAW, IPPROTO_UDP);
	struct sockaddr_in to;
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = inet_addr(argv[3]);
	to.sin_port = htons(atoi(argv[4]));
	if ( bind(sd , (struct sockaddr *)&to, sizeof(to)) < 0){
		printf(L"bind failed with error \n");
		return 1;
	}
	if(sd < 0)
	{
		perror("socket() error");
		// If something wrong just exit
		exit(-1);
	}
	else
		printf("socket() - Using SOCK_RAW socket and UDP protocol is OK.\n");


	int x = 0;
	while ( 1 )
	{
		unsigned char packet_data[256];

		unsigned int max_packet_size = 
			sizeof( packet_data );

		typedef int socklen_t;

		struct sockaddr_in from;
		socklen_t fromLength = sizeof( from );
		from.sin_family = AF_INET;
		from.sin_addr.s_addr = inet_addr(argv[1]);
		from.sin_port = htons(atoi(argv[2]));

		printf("trying to receive\n");
		int bytes = recvfrom( sd, 
				(char*)packet_data, 
				max_packet_size,
				0, 
				(struct sockaddr_in*)&from, 
				&fromLength );

		if ( bytes <= 0 )
			break;
		printf("received %d %s\n", ++x, packet_data);

		unsigned int from_address = 
			ntohl( from.sin_addr.s_addr );

		unsigned int from_port = 
			ntohs( from.sin_port );

		// process received packet
	}
}
