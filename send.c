#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include "struct.h"

#define SRV_IP "127.0.0.1"
#define BUFLEN 512
#define NPACK 10
#define PORT 2000

/* diep(), #includes and #defines like in the server */
void diep(char *s)
{
	perror(s);
	exit(1);
}

int main(int argc, char *argv[])
{
	if(argc != 5)
	{
		printf("- Invalid parameters!!!\n");
		printf("- Usage %s <source hostname/IP> <source port> <target hostname/IP> <target port>\n", argv[0]);
		exit(-1);
	}
	
	char *dstIP = argv[3];
	struct sockaddr_in si_other;
	int s, i, slen=sizeof(si_other);
	char buf[BUFLEN];
	bzero(buf, BUFLEN);
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	p_tcphdr->src_port = atoi(argv[2]);
	p_tcphdr->dst_port = atoi(argv[4]);
	p_tcphdr->flags = FLAG_SYN;

	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);
	if (inet_aton(dstIP, &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	for (i=0; i<NPACK; i++) {
		printf("Sending packet %d\n", i);
		sprintf(buf, "This is packet %d\n", i);
		if (sendto(s, buf, BUFLEN, 0, &si_other, slen)==-1)
			diep("sendto()");
	}

	close(s);
	return 0;
}

