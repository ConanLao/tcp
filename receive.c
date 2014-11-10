#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "struct.h"
#define BUFLEN 512
#define NPACK 10
#define PORT 5000

void diep(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	uint16_t src_port;
	uint16_t dst_port;
	struct sockaddr_in si_me, si_other;
	struct timeval tv;
	int s, i, slen=sizeof(si_other);
	char buf[BUFLEN];
	int flag; 
	int states[6][6] =
	{
		{2,0,0,0,0}, //waiting for SYN
		{0,0,0,0,0}, //waiting for SYN_ACK
		{2,3,0,0,0}, //waiting for ACK
		{0,0,0,0,0}, //CONNECTED 
		{0,0,0,0,0}, //CLOSED
		{0,0,0,0,0} //FIN_RECEIVED
	};
	int state = 0;
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s, &si_me, sizeof(si_me))==-1)
		diep("bind");
	tv.tv_sec = 1;
	int resend = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	while(1)
	{	
		int received_size =0;
		if ( (received_size =recvfrom(s, buf, BUFLEN, 0, &si_other, &slen))<0)
		{
			//printf("sent SYN/ACK");
			if(resend >7) 
			{
				printf("connection failed\n");
				state=0;
				resend =0;
				continue;
			}
			if(state ==2)
			{
				resend++;
				bzero(buf, BUFLEN);
				p_tcphdr->flags = FLAG_SYN | FLAG_ACK;
				pack_uint16(dst_port,p_tcphdr->src_port);
				pack_uint16(src_port,p_tcphdr->dst_port);
				si_other.sin_family = AF_INET;
				si_other.sin_port = htons(src_port);
				if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
					diep("sendto()");
				printf("sent SYN/ACK");
				continue;
			}
			else 
				continue;
		}	
		tcp_header_t* tcp_header =(tcp_header_t *) buf;
		src_port = unpack_uint16(tcp_header->src_port);
		dst_port = unpack_uint16(tcp_header->dst_port);
		flag = tcp_header->flags;
		int input =0;
		if( (flag & FLAG_SYN) && (flag & FLAG_ACK)==0) input =0;
		else if((flag & FLAG_SYN)==0 && (flag & FLAG_ACK)) input =1;
		else if((flag & FLAG_SYN) && (flag & FLAG_ACK)) input =2;
		else if((flag & FLAG_FIN) && (flag & FLAG_ACK)==0) input =3;
		else if(flag & FLAG_RST) input =4;
		state = states[state][input];
		if(state == 2)
		{
			bzero(buf, BUFLEN);
			p_tcphdr->flags = FLAG_SYN | FLAG_ACK;
			pack_uint16(dst_port,p_tcphdr->src_port);
			pack_uint16(src_port,p_tcphdr->dst_port);
			si_other.sin_family = AF_INET;
			si_other.sin_port = htons(src_port);
			if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
				diep("sendto()");
			printf("sent SYN/ACK");
		}
		if(state == 3)
		{
			printf("connected\n");
		}
		printf("src_port :%d\n dst_port:%d\n\n",src_port,dst_port); 
		printf("flag :%d\n",tcp_header->flags); 
		printf("Received packet from %s:%d\nData: %s\n\n", 
				inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
		printf("state is %d\n",state);
	}


	close(s);
	return 0;
}
