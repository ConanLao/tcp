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

//char* dst_ip;
//uint16_t dst_port;
//uint16_t src_port;
//int state = WAITING_FOR_SYNACK;
//int seq = 1;//should be 0 considering the handshake
//int ack = 0;


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

int send_udp(int type, char* data, int len, long seq) {
	struct sockaddr_in si_other;
	int s, i, slen=sizeof(si_other);
	char buf[BUFLEN];
	bzero(buf, BUFLEN);
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	memcpy(buf+sizeof(tcp_header_t), data, len);
	pack_uint16(src_port, p_tcphdr->src_port);
	pack_uint16(dst_port, p_tcphdr->dst_port);
	p_tcphdr->flags = type;
	pack_uint32(seq, p_tcphdr->seq_num);

	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(dst_port);
	if (inet_aton(dst_ip, &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
	if (sendto(s, (char*)buf, sizeof(tcp_header_t) + len, 0, &si_other, slen)==-1)
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
		send_udp(FLAG_SYN, "", 0, 0);
		printf("receiving SYN No.%d\n", i);
		num = receive_udp(buf, tv, si_other);
		printf("after\n");
		if (num >= sizeof(tcp_header_t)) {
			tcp_header_t* tcp_header =(tcp_header_t *) buf;
			printf("synack : %d\n", tcp_header->flags);//need to check the ip is the server or not
			printf("src = %d\n", unpack_uint16(tcp_header->src_port));
			printf("dst = %d\n", unpack_uint16(tcp_header->dst_port));
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
	for(i = 0;i<1;i++) {
		printf("sending ack No.%d\n", i);
		send_udp(FLAG_ACK, "", 0, -1);
		sleep(1);
	}
	return 0;
}

int sonic_connect() {
	if (state != WAITING_FOR_SYNACK)
		printf("connect failed: state fault\n");
	printf("state = %d\n", state);
	return listen_to_synack();
}

int tcp_send(char* data, int len){
	if (len == 0) {
		return 1;
	}
	printf("tcp sending data\n");
	struct sockaddr_in si_other;
	char buf[BUFLEN];
	struct timeval tv;
	tv.tv_sec = 1;
	long next_seq = len + seq;
		int i;
		for(i = 0;i<7;i++) {
		send_udp(FLAG_DATA, data, len, seq);
		printf("receiving data No.%d\n", i);
		int num = receive_udp(buf, tv, si_other);
		if (num >= sizeof(tcp_header_t)) {
			tcp_header_t* tcp_header =(tcp_header_t *) buf;
			printf("data : %d\n", tcp_header->flags);//need to check the ip is the server or not
			printf("src = %d\n", unpack_uint16(tcp_header->src_port));
			printf("dst = %d\n", unpack_uint16(tcp_header->dst_port));
			printf("ack = %ld\n", unpack_uint32(tcp_header->ack_num));
			if (src_port == unpack_uint16(tcp_header->dst_port)
					&& dst_port == unpack_uint16(tcp_header->src_port)
					&& tcp_header->flags == FLAG_ACK
					&& unpack_uint32(tcp_header->ack_num) == next_seq){//need to check the ip is the server or not
				seq = next_seq;
				return 1;
			}
		}
	}
	return 0;
}

int sonic_close(){
	struct sockaddr_in si_other;
	int num = -2;
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	char *buf = calloc(1, BUFLEN);
	int i;
	for(i = 0;i<7;i++) {
		printf("sending FIN No.%d\n", i);
		send_udp(FLAG_FIN, "", 0, -1);
		printf("receiving FINACK No.%d\n", i);
		num = receive_udp(buf, tv, si_other);
		if (num >= sizeof(tcp_header_t)) {
			printf("a\n");
			tcp_header_t* tcp_header =(tcp_header_t *) buf;
			printf("finack : %d\n", tcp_header->flags);//need to check the ip is the server or not
			printf("src = %d\n", unpack_uint16(tcp_header->src_port));
			printf("dst = %d\n", unpack_uint16(tcp_header->dst_port));
			if (src_port == unpack_uint16(tcp_header->dst_port)
					&& dst_port == unpack_uint16(tcp_header->src_port)
					&& tcp_header->flags == FLAG_FINACK){//need to check the ip is the server or not
				printf("b\n");
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
	//create_client();
	return;
	sonic_close();	
	return 0;
}

