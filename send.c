#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "struct.h"

#define BUFLEN 512

char* dst_ip;
uint16_t dst_port;
uint16_t src_port;
int state = WAITING_FOR_SYNACK;

/* diep(), #includes and #defines like in the server */
void diep(char *s)
{
	perror(s);
	exit(1);
}

int receive_udp(char* buf, struct timeval tv, struct sockaddr_in si_other){
	struct sockaddr_in si_me;
	int s, i, slen=sizeof(si_other);
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(src_port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s, &si_me, sizeof(si_me))==-1)
		diep("bind");
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
	printf("a\n");
	int num = recvfrom(s, buf, BUFLEN, 0, &si_other, &slen);
	printf("a\n");
	//tcp_header_t* tcp_header =(tcp_header_t *) buf;
	//src_port = unpack_uint16(tcp_header->src_port);
	//dst_port = unpack_uint16(tcp_header->dst_port);
	//printf("src_port :%d\n dst_port:%d\n\n",src_port,dst_port); 
	//printf("flag :%d\n",tcp_header->flags); 
	//printf("Received packet from %s:%d\nData: %s\n\n", 
	//		inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
	close(s);
	return num;
}

int send_udp(int type, char* data) {
	struct sockaddr_in si_other;
	int s, i, slen=sizeof(si_other);
	char buf[BUFLEN];
	bzero(buf, BUFLEN);
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	pack_uint16(src_port, p_tcphdr->src_port);
	pack_uint16(dst_port, p_tcphdr->dst_port);
	p_tcphdr->flags = type;

	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(dst_port);
	if (inet_aton(dst_ip, &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
	if (sendto(s, (char*)p_tcphdr, BUFLEN, 0, &si_other, slen)==-1)
		diep("sendto()");
	close(s);
	return 1;

}

int listen_to_synack(){
	struct sockaddr_in si_other;
	int num = -2;
	struct timeval tv;
	tv.tv_sec = 1;
	char *buf = calloc(1, BUFLEN);
	int i;
	for(i = 0;i<7;i++) {
		printf("sending SYN No.%d\n", i);
		send_udp(FLAG_SYN, "");
		printf("receiving SYN No.%d\n", i);
		num = receive_udp(buf, tv, si_other);
		printf("after\n");
		if (num >= sizeof(tcp_header_t)) {
			tcp_header_t* tcp_header =(tcp_header_t *) buf;
			if (src_port == unpack_uint16(tcp_header->dst_port)
					&& dst_port == unpack_uint16(tcp_header->src_port)
					&& tcp_header->flags == FLAG_SYNACK){//need to check the ip is the server or not
				state = CONNECTED;
				return 1;
			}
		}
	}
	return 0;
}

int send_ack(){
	struct sockaddr_in si_other;
	int num = -2;
	struct timeval tv;
	tv.tv_sec = 1;
	int i;
	for(i = 0;i<7;i++) {
		printf("sending ack No.%d\n", i);
		send_udp(FLAG_ACK, "");
		sleep(1);
	}
	return 0;
}

int sonic_connect() {
	if (state != WAITING_FOR_SYNACK)
		printf("connect failed: state fault\n");
	int reulst = listen_to_synack();
	printf("state = %d\n", state);
	return 1;	
}

int sonic_close(){
	struct sockaddr_in si_other;
	int num = -2;
	struct timeval tv;
	tv.tv_sec = 1;
	char *buf = calloc(1, BUFLEN);
	int i;
	for(i = 0;i<7;i++) {
		printf("sending FYN No.%d\n", i);
		send_udp(FLAG_FIN, "");
		printf("receiving FYNACK No.%d\n", i);
		num = receive_udp(buf, tv, si_other);
		if (num >= sizeof(tcp_header_t)) {
			tcp_header_t* tcp_header =(tcp_header_t *) buf;
			if (src_port == unpack_uint16(tcp_header->dst_port)
					&& dst_port == unpack_uint16(tcp_header->src_port)
					&& tcp_header->flags == FLAG_FINACK){//need to check the ip is the server or not
				state = CLOSED;
				return 1;
			}
		}
	}
	return 0;

	
}

int main(int argc, char *argv[])
{
	if(argc != 5)
	{
		printf("- Invalid parameters!!!\n");
		printf("- Usage %s <source hostname/IP> <source port> <target hostname/IP> <target port>\n", argv[0]);
		exit(-1);
	}
	dst_ip = argv[3];
	src_port = atoi(argv[2]);
	dst_port = atoi(argv[4]);
	printf("Using Source IP: %s port: %u, Target IP: %s port: %u.\n", argv[1], atoi(argv[2]), argv[3], atoi(argv[4]));
	int result = sonic_connect();	
	if (!result) {
		return 0;
	}

	send_ack();
	sonic_close();	
	return 0;
}

