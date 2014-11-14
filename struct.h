#include "list.h"
#include <semaphore.h>
#include <pthread.h>

// The packet length
#define BUF_LEN 1600 //MTU of ehternet is 1500 bytes 

//TCP flags
#define FLAG_DATA 0
#define FLAG_FIN 1
#define FLAG_SYN 2
#define FLAG_RST 4
#define FLAG_PSH 8
#define FLAG_ACK 16
#define FLAG_URG 32
#define FLAG_ECE 64
#define FLAG_CWR 128
#define FLAG_NS 256
#define FLAG_SYNACK 18
#define FLAG_FINACK 17
#define PORT 5000

enum socket_type { CLIENT = 0, SERVER };

enum socket_state {
	WAITING_FOR_SYN = 0, 
	WAITING_FOR_SYNACK, 
	WAITING_FOR_ACK, 
	CONNECTED,
	WAITING_FOR_FIN,
	CLOSED
};

// Can create separate header file (.h) for all headers' structure
// The IP header's structure
struct ipheader {
	unsigned char      iph_ihl:5, iph_ver:4;
	unsigned char      iph_tos;
	unsigned short int iph_len;
	unsigned short int iph_ident;
	unsigned char      iph_flag;
	unsigned short int iph_offset;
	unsigned char      iph_ttl;
	unsigned char      iph_protocol;
	unsigned short int iph_chksum;
	unsigned int       iph_sourceip;
	unsigned int       iph_destip;
};

// UDP header's structure
struct udpheader {
	unsigned short int udph_srcport;
	unsigned short int udph_destport;
	unsigned short int udph_len;
	unsigned short int udph_chksum;
	uint8_t data[0];
};

/*
 *struct for the tcp header
 */
typedef struct    
{
	uint8_t src_port[2];
	uint8_t dst_port[2];
	uint8_t seq_num[4];
	uint8_t ack_num[4];
	uint8_t data_res_ns;
	uint8_t flags;
	uint8_t window[2];
	uint8_t checksum[2];
	uint8_t urgent_p[2];
	uint8_t options_and_data[0];
}tcp_header_t;

struct send_list{
	char* data;
	int len;
	uint8_t flags;
	uint32_t seq;
	uint32_t ack;
	uint16_t window;
	struct list_head list;
};

char* dst_ip;
uint16_t dst_port;
uint16_t src_port;
int state;
uint32_t seq;
uint32_t ack;
uint16_t window = 1;
struct sockaddr_in si_me;
struct sockaddr_in si_other;
int s;
struct send_list mylist;
sem_t sender_sema;
sem_t list_sema;

void pack_uint16(uint16_t val, uint8_t* buf) {
	val = htons(val);
	memcpy(buf, &val, sizeof(uint16_t));
}

uint16_t unpack_uint16(const uint8_t* buf) {
	uint16_t val;
	memcpy(&val, buf, sizeof(uint16_t));
	return ntohs(val);
}

void pack_uint32(uint32_t val, uint8_t* buf) {
	val = htonl(val);
	memcpy(buf, &val, sizeof(uint32_t));
}

uint32_t unpack_uint32(const uint8_t* buf) {
	uint32_t val;
	memcpy(&val, buf, sizeof(uint32_t));
	return ntohl(val);
}



int add_send_task(char* data, int len, uint8_t flags, uint32_t seq, uint32_t ack, uint16_t window){
	printf("add begin\n");
	struct send_list *tmp;
	tmp= (struct send_list *)malloc(sizeof(struct send_list));
	tmp->data = data;
	tmp->len = len;
	tmp->flags = flags;
	tmp->seq = seq;
	tmp->ack = ack;
	tmp->window = window;
	sem_wait( &list_sema);
	list_add_tail(&(tmp->list), &(mylist.list));
	sem_post( &list_sema);
	sem_post( &sender_sema);
	printf("add end\n");
}

//int len is the length of the udp payload
int udp_send(char* data, int len) {
	int slen=sizeof(si_other);
	char buf[len];//opt
	bzero(buf, len);
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	memcpy(buf, data, len);
	if (sendto(s, (char*)buf, len, 0, &si_other, slen)==-1)
		printf("[error] sendto()");
	return 1;
}

int send_tcp(char* data,int len, int flags, uint32_t seq, uint32_t ack, uint16_t window){
	char buf[len + sizeof(tcp_header_t)];
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	pack_uint16(src_port, p_tcphdr->src_port);
	pack_uint16(dst_port, p_tcphdr->dst_port);
	p_tcphdr->flags = flags;
	pack_uint32(seq, p_tcphdr->seq_num);
	pack_uint32(ack, p_tcphdr->ack_num);
	pack_uint16(ack, p_tcphdr->window);
	memcpy(p_tcphdr->options_and_data, data, len);
	return udp_send(buf, len + sizeof(tcp_header_t));
}

int udp_receive(char* buf, struct sockaddr_in si_other){
	int slen=sizeof(si_other);
	int num = recvfrom(s, buf, BUF_LEN, 0, &si_other, &slen);
	//tcp_header_t* tcp_header =(tcp_header_t *) buf;
	//src_port = unpack_uint16(tcp_header->src_port);
	//dst_port = unpack_uint16(tcp_header->dst_port);
	//printf("src_port :%d\n dst_port:%d\n\n",src_port,dst_port); 
	//printf("flag :%d\n",tcp_header->flags); 
	//printf("Received packet from %s:%d\nData: %s\n\n", 
	//              inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
	return num;
}


void *thread_send(void *arg){
	printf("send thread started\n");
	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(dst_port);
	if (inet_aton(dst_ip, &si_other.sin_addr)==0) {
		printf("[error] inet_aton() failed\n");
	}
	while(1){
		sem_wait( &sender_sema );
		sem_wait( &list_sema );	
		struct send_list *tmp =list_entry(mylist.list.next, struct send_list, list);	
		//send_tcp(sock_fd, si_other, len, data, flags, seq, ack);
		send_tcp(tmp->data, tmp->len, tmp->flags, tmp->seq, tmp->ack, tmp->window);
		list_del(mylist.list.next);
		sem_post( &list_sema );	
	}
}

int create_client(char* d_ip, uint16_t d_port, uint16_t s_port){
	dst_ip = d_ip;
	dst_port = d_port;
	src_port = s_port;
	sem_init( &sender_sema, 0,0);
	sem_init( &list_sema, 0,1);


	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");
	
	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(src_port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(s, &si_me, sizeof(si_me))==-1)
		diep("bind");

	printf("a\n");
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
	printf("b\n");

	INIT_LIST_HEAD(&mylist.list);
	pthread_t send_thread;
	printf("c\n");
	if(pthread_create( &send_thread, NULL, thread_send, NULL) ){
		fprintf(stderr, "Error in creating thread\n");
		return 0;
	}
	printf("d\n");
/**
	char buf[BUF_LEN];
	for(i = 0;i<7;i++) {
		printf("sending SYN No.%d\n", i);
		add_send_task("",0,FLAG_SYN, seq, ack, window);
		printf("receiving SYNACK No.%d\n", i);
		num = udp_receive(buf);
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

*/

	int i =0;
	for(i = 0; i<256;i++){
		add_send_task("0123456789", 10 , i,i, i, i);
	}

}
int create_server()
{
	dst_ip = d_ip;
	dst_port = d_port;
	src_port = s_port;
	sem_init( &sender_sema, 0,0);
	sem_init( &list_sema, 0,1);

	if ((fd_me=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		printf("socket");
	
	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd_me, &si_me, sizeof(si_me))==-1)
		printf("bind");
	}
	INIT_LIST_HEAD(&mylist.list);
	pthread_t send_thread;
	printf("c\n");
	if(pthread_create( &send_thread, NULL, thread_send, NULL) ){
		fprintf(stderr, "Error in creating thread\n");
		return 0;
	}
	printf("d\n");
}
