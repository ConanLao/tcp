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
#define MAX_WINDOW 65536 //2^16

void diep(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	create_server();
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
		{2,3,0,2,0}, //waiting for ACK
		{3,3,3,0,0}, //CONNECTED 
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
			if(resend >7) 
			{
				printf("connection failed\n");
				state=0;
				resend =0;
				continue;
			}
			if(state ==2)
			{
				uint32_t seq = unpack_uint32(p_tcphdr->seq_num);
				uint32_t ack = unpack_uint32(p_tcphdr->ack_num);
				resend++;
				bzero(buf, BUFLEN);
				p_tcphdr->flags = FLAG_SYN | FLAG_ACK;
				p_tcphdr->window = MAX_WINDOW;
				pack_uint16(dst_port,p_tcphdr->src_port);
				pack_uint16(src_port,p_tcphdr->dst_port);
				si_other.sin_family = AF_INET;
				si_other.sin_port = htons(src_port);
				// if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
				// 	diep("sendto()");
				add_send_task("", 0 , FLAG_SYN | FLAG_ACK ,seq, ack,MAX_WINDOW);
				printf("sent SYN/ACK");
				continue;
			}
			else 
				continue;
		}	
		tcp_header_t* tcp_header =(tcp_header_t *) buf;
		printf("Received packet from %s:%d\nData: %s\n\n", 
				inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), (char*)tcp_header->options_and_data);
		printf("flag :%d\n",tcp_header->flags); 
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
		if( (flag & FLAG_FIN) && (state!=3)) 
		{
			uint32_t seq = unpack_uint32(p_tcphdr->seq_num);
			uint32_t ack = unpack_uint32(p_tcphdr->ack_num);
			bzero(buf, BUFLEN);
			p_tcphdr->flags = FLAG_FIN | FLAG_ACK;
			p_tcphdr->window = MAX_WINDOW;
			pack_uint16(dst_port,p_tcphdr->src_port);
			pack_uint16(src_port,p_tcphdr->dst_port);
			si_other.sin_family = AF_INET;
			si_other.sin_port = htons(src_port);
			// if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
			// 	diep("sendto()");
			// data, length,flag, seq, ack, window
			add_send_task("", 0 , FLAG_FIN | FLAG_ACK ,seq, ack,MAX_WINDOW);
			printf("sent FIN");
			continue;
		}
		if(state == 2)
		{
			uint32_t seq = unpack_uint32(p_tcphdr->seq_num);
			uint32_t ack = unpack_uint32(p_tcphdr->ack_num);
			bzero(buf, BUFLEN);
			p_tcphdr->flags = FLAG_SYN | FLAG_ACK;
			p_tcphdr->window = MAX_WINDOW;
			si_other.sin_family = AF_INET;
			si_other.sin_port = htons(src_port);
			pack_uint16(dst_port,p_tcphdr->src_port);
			pack_uint16(src_port,p_tcphdr->dst_port);
			// if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
			// 	diep("sendto()");
			add_send_task("", 0 , FLAG_SYN | FLAG_ACK ,seq, ack,MAX_WINDOW);
			printf("sent SYN/ACK\n");
		}
		if(state == 3)
		{
			uint32_t seq = unpack_uint32(p_tcphdr->seq_num);
			uint32_t ack = unpack_uint32(p_tcphdr->ack_num);
			uint16_t check_sum =unpack_uint16(p_tcphdr->checksum);
			int size = received_size - 20;
			bzero(buf, BUFLEN);
			p_tcphdr->flags =FLAG_ACK;
			p_tcphdr->window = MAX_WINDOW;
			si_other.sin_family = AF_INET;
			si_other.sin_port = htons(src_port);
			pack_uint16(dst_port,p_tcphdr->src_port);
			pack_uint16(src_port,p_tcphdr->dst_port);
			pack_uint32(seq+size,p_tcphdr->ack_num);
			// if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
			//  	diep("sendto()");
			dd_send_task("", 0 , FLAG_ACK ,seq, ack,MAX_WINDOW);
			printf("connected\n");
		}
		printf("src_port :%d\n dst_port:%d\n\n",src_port,dst_port); 
		printf("state is %d\n",state);
	}


	close(s);
	return 0;
}
