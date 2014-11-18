#include "list.h"
#include <semaphore.h>
#include <pthread.h>
#include <time.h>

// The packet length
#define PCKT_LEN 1000
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

#define RECEIVE_BUF_SIZE 1024 * 1024 * 32 // 32 mb

enum socket_type { CLIENT = 0, SERVER };

enum socket_state {
	WAITING_FOR_SYN = 0, 
	WAITING_FOR_SYNACK, 
	WAITING_FOR_ACK, 
	CONNECTED,
	WAITING_FOR_FIN,
	CLOSED
};
enum type {
	TYPE_SERVER = 0,
	TYPE_CLIENT
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

char* test_data = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
char* dst_ip;
uint16_t dst_port;
uint16_t src_port;
int type;
int state;
uint32_t seq;
uint32_t ack;
uint16_t window = 1;
struct sockaddr_in si_me;
struct sockaddr_in si_other;
int s;
struct send_list mylist;
sem_t create_sema;// this is used to make sure the create function returns after the threads are created
sem_t sender_sema; 
sem_t list_sema;
sem_t state_sema;

char* receive_buf;
unsigned int buf_s = 0;//first available position
unsigned int buf_len = 0;

pthread_t receive_thread;
pthread_t send_thread;
pthread_t resend_thread;

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

int copy_to_buf(char* data, int len) {
	int a = 0;
	int b = 0;
	int start = 0;
	if ( (buf_s + buf_len) < RECEIVE_BUF_SIZE) {
		if ( (RECEIVE_BUF_SIZE - buf_s - buf_len) > len) {
			memcpy(receive_buf+buf_s+buf_len, data, len);
			buf_len += len;
			return len;
		} else {
			a = RECEIVE_BUF_SIZE - buf_s - buf_len;
			memcpy(receive_buf+buf_s+buf_len, data, a);
			b = len - a;
			memcpy(receive_buf, data + a, b);
			buf_len +=len;
			return len;
		} 
	} else {
		start = buf_s + buf_len - RECEIVE_BUF_SIZE;
		memcpy(receive_buf + start, data, len );
		buf_len += len;
		return len;
	}
}

int read_from_buf(char* dst, int len) {
	int a = 0;
	int b = 0;
	int read_len = 0;

	if (buf_len <= len) {
		read_len = buf_len;
	} else {
		read_len = len;
	}

	if ((buf_s + buf_len) <= RECEIVE_BUF_SIZE) {
		memcpy(dst, receive_buf+buf_s, read_len);
		buf_s += read_len;
		buf_len -= read_len;
		return read_len;
	} else {
		if ((RECEIVE_BUF_SIZE - buf_s)>=read_len ){
			memcpy(dst, receive_buf+buf_s, read_len);
			buf_s += read_len;
			buf_len -= read_len;
			return read_len;
		} else {
			a = RECEIVE_BUF_SIZE - buf_s;
			memcpy(dst, receive_buf+buf_s, a);
			buf_s = 0;
			buf_len -= a;
			b = read_len - a;
			memcpy(dst, receive_buf, b);
			buf_s += b;
			buf_len -= b;	
			return read_len;
		}
	} 
}

int add_send_task(char* data, int len, uint8_t flags, uint32_t seq, uint32_t ack, uint16_t window){
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
	return 1;
}

//int len is the length of the udp payload
int udp_send(char* data, int len) {
	int slen=sizeof(si_other);
	char buf[len];//opt
	bzero(buf, len);
	memcpy(buf, data, len);
	//printf("udp send dst ip = %s\n", inet_ntoa(si_other.sin_addr));

	if (sendto(s, (char*)buf, len, 0, (struct sockaddr *)&si_other, slen)==-1)
		printf("[error] sendto()");
	return 1;
}

int tcp_send(char* data,int len, int flags, uint32_t seq, uint32_t ack, uint16_t window){
	char buf[len + sizeof(tcp_header_t)];
	tcp_header_t *p_tcphdr = (tcp_header_t *)buf;
	pack_uint16(src_port, p_tcphdr->src_port);
	pack_uint16(dst_port, p_tcphdr->dst_port);
	p_tcphdr->flags = flags;
	pack_uint32(seq, p_tcphdr->seq_num);
	pack_uint32(ack, p_tcphdr->ack_num);
	pack_uint16(window, p_tcphdr->window);

	memcpy(p_tcphdr->options_and_data, data, len);
	return udp_send(buf, len + sizeof(tcp_header_t));
}

int udp_receive(char* buf, struct sockaddr_in si_other){
	int slen=sizeof(si_other);
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec=0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	int num = recvfrom(s, buf, BUF_LEN, 0, (struct sockaddr *)&si_other, &slen);

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

	sem_post(&create_sema);
	while(1){
		sem_wait( &sender_sema ); 
		sem_wait( &list_sema );	
		struct send_list *tmp =list_entry(mylist.list.next, struct send_list, list);	
		//send_tcp(sock_fd, si_other, len, data, flags, seq, ack);
		tcp_send(tmp->data, tmp->len, tmp->flags, tmp->seq, tmp->ack, tmp->window);
		list_del(mylist.list.next);
		sem_post( &list_sema );	
	}
}

void *thread_receive(void *arg){
	printf("receive thread started\n");
	struct sockaddr_in si_dum;

	char buf[BUF_LEN];
	int num;
	sem_post(&create_sema);
	int resend = 0;
	int ack_l=0, seq_l =0;
	while(1){
		num = udp_receive(buf, si_dum);
		tcp_header_t* tcp_header =(tcp_header_t *) buf;
		if(num<20)
		{
			if(resend >7) 
			{
				printf("connection failed\n");
				state=0;
				resend =0;
				continue;
			}
			if(state == WAITING_FOR_ACK)
			{
				seq_l = unpack_uint32(tcp_header->seq_num);
				ack_l = unpack_uint32(tcp_header->ack_num);
				if(ack != seq_l) continue; 
				ack  = seq_l+1;
				resend++;
				add_send_task("", 0 , FLAG_SYN | FLAG_ACK ,seq, ack,window);
				continue;
			}
			else 
				continue;
		}
		if (num >= 20 ) {

			printf("[receive] flag = %d, seq = %d, ack = %d\n",tcp_header->flags, unpack_uint32(tcp_header->seq_num), unpack_uint32(tcp_header->ack_num) );
			if(tcp_header->flags == FLAG_SYN && type ==TYPE_SERVER)
			{	
				dst_port = unpack_uint16(tcp_header->src_port);
				dst_ip = "128.84.139.25";
				if (inet_aton(dst_ip, &si_other.sin_addr)==0) {
					printf("[error] inet_aton() failed\n");
				}
				//dst_ip = inet_ntoa(si_dum.sin_addr);
				printf("[tcp_receive:]dst ip:%s,dst_port:%d\n",dst_ip,dst_port);
				seq_l = unpack_uint32(tcp_header->seq_num);
				ack_l = unpack_uint32(tcp_header->ack_num);
				//if(state == WAITING_FOR_SYN && ack != seq_l) continue; 
				if (state == WAITING_FOR_SYN) {	
					state = WAITING_FOR_ACK;
					ack  = seq_l+1;
				}
				add_send_task("", 0 , FLAG_SYN | FLAG_ACK ,seq, ack,window);
				continue;
				}
				if (src_port != unpack_uint16(tcp_header->dst_port)
						|| dst_port != unpack_uint16(tcp_header->src_port)){//wrong connection; need to check source addr acutally but you know..
					continue;
				}
				if(tcp_header->flags == FLAG_FIN && state!=WAITING_FOR_FIN)
				{
					int size = num - 20;
					add_send_task("", 0 , FLAG_FIN | FLAG_ACK ,0, seq+size,window);
					if(type== TYPE_CLIENT) 
						state = CLOSED;
					continue;
				}
				if(tcp_header->flags == FLAG_ACK && state == WAITING_FOR_ACK && type == TYPE_SERVER)
				{
					seq_l = unpack_uint32(tcp_header->seq_num);
					ack_l = unpack_uint32(tcp_header->ack_num);
					if(ack != seq_l) continue; 
					//ack  = seq_l+1;
					add_send_task("", 0 , FLAG_ACK ,seq, ack,window);
					printf("connected\n");
					state = CONNECTED;
					continue;
				}
				sem_wait(&state_sema);
				//resending ack in three way hand shake
				if (tcp_header->flags == FLAG_SYNACK){//to do: check current state
					//printf("[receive]received: SYNACK\n");
					add_send_task("", 0, FLAG_ACK, 1, unpack_uint32(tcp_header->seq_num)+1, window);
				} else if(tcp_header->flags == FLAG_FINACK && state == WAITING_FOR_FIN) {
					//printf("[receive]received: FINACK\n");
					state = CLOSED;
				}	
				sem_post(&state_sema);
				if(tcp_header->flags == 0 && state == CONNECTED)
				{	
					//printf("[receive thread] data received\n");
					seq_l = unpack_uint32(tcp_header->seq_num);
					ack_l = unpack_uint32(tcp_header->ack_num);
					//printf("[receive thread] global ack = %d, seq_l = %d\n", ack, seq_l);
					if(ack != seq_l) continue; 
					int size = num -20;
					ack  = seq_l+size;
					//printf("[received thread] sending back ack; seq = %d, ack = %d\n", seq, ack);
					add_send_task("", 0 , FLAG_ACK ,seq, ack,window);
				}
				if(tcp_header->flags == FLAG_ACK && state == CONNECTED)
				{	
					//printf("[receive thread] data ack received\n");
					seq_l = unpack_uint32(tcp_header->seq_num);
					ack_l = unpack_uint32(tcp_header->ack_num);
					//printf("[receive thread] data ack global ack = %d, seq_l = %d", seq, ack_l);
					if (seq < ack_l){
						seq = ack_l;
					}
				}
			}
		}

		//udp_receive(buf, si_dum);
		return NULL;
	}

	void *thread_resend(void *arg){
		printf("resend thread started\n");
		sem_post(&create_sema);
		return NULL;
	}

	int create_client(char* d_ip, uint16_t d_port, uint16_t s_port){
		dst_ip = d_ip;
		dst_port = d_port;
		src_port = s_port;
		type = TYPE_CLIENT;
		sem_init( &create_sema,0, 0);
		sem_init( &sender_sema, 0,0);
		sem_init( &list_sema, 0,1);
		sem_init( &state_sema, 0, 1);
		printf("creating TCP client with dst_port:%d, src_port:%d\n",dst_port, src_port );

		if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1);//diep("socket");

		memset((char *) &si_me, 0, sizeof(si_me));
		si_me.sin_family = AF_INET;
		si_me.sin_port = htons(src_port);
		si_me.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1);//diep("bind");G_ACK 16

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

		INIT_LIST_HEAD(&mylist.list);
		if(pthread_create( &send_thread, NULL, thread_send, NULL) ){
			fprintf(stderr, "Error in creating send thread\n");
			return 0;
		}

		struct sockaddr_in si_dum;
		char buf[BUF_LEN];
		int i, num;
		int flag=0;
		for(i = 0;i<7;i++) {
			add_send_task("",0,FLAG_SYN, seq, ack, window);

			num = udp_receive(buf, si_dum);
			if (num >= 20 ) {
				tcp_header_t* tcp_header =(tcp_header_t *) buf;
				if (src_port == unpack_uint16(tcp_header->dst_port)
						&& dst_port == unpack_uint16(tcp_header->src_port)
						&& tcp_header->flags == FLAG_SYNACK){//need to check the ip is the server or not
					ack = unpack_uint32(tcp_header->seq_num);
					seq = unpack_uint32(tcp_header->ack_num);
					printf("SEQ: %d\n",seq);
					sem_wait(&create_sema);
					state = CONNECTED;
					sem_post(&create_sema);
					add_send_task("", 0, FLAG_ACK, 1, unpack_uint32(tcp_header->seq_num)+1, window);
					flag=1;
					break;
				}
			}
		}
		if(flag==0) 
		{
			printf("connection failed! Exiting..");
			return -1;
		}

		printf("[creating client] SYNACK received\n");
		receive_buf = (char *)malloc(RECEIVE_BUF_SIZE);


		if(pthread_create( &receive_thread, NULL, thread_receive, NULL) ){
			fprintf(stderr, "Error in creating receive thread\n");
			return 0;
		}

		if(pthread_create( &resend_thread, NULL, thread_resend, NULL) ){
			fprintf(stderr, "Error in creating resend thread\n");
			return 0;
		}

		//for(i = 0;i<7;i++) {
		//	printf("sending ACK(SYN) No.%d\n", i);
		//	add_send_task("",0,FLAG_ACK, 1, 0, window);//ack may not be 0
		//	printf("receiving ACK No.%d\n", i);
		//	udp_receive(buf, si_dum);
		//}
		sem_wait(&create_sema);
		sem_wait(&create_sema);
		sem_wait(&create_sema);
		printf("[creating client] successful\n");
		/**
		  for(i = 0; i<256;i++){
		  printf("i=%d\n",i);
		  add_send_task("0123456789", 10 , 0,i, i, i);
		  udp_receive(buf, si_dum);
		  }
		 */

		return 1;
	}
	int create_server(uint16_t src_p)
	{
		dst_ip = "127.0.0.1";
		dst_port = 3000;
		src_port = src_p;
		type =TYPE_SERVER;
		sem_init( &sender_sema, 0,0);
		sem_init( &list_sema, 0,1);
		sem_init( &create_sema,0, 0);
		sem_init( &state_sema, 0, 1);

		if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
			printf("socket");
		memset((char *) &si_me, 0, sizeof(si_me));
		si_me.sin_family = AF_INET;
		si_me.sin_port = htons(PORT);
		si_me.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1)
		{
			printf("bind");
		}
		INIT_LIST_HEAD(&mylist.list);
		pthread_t send_thread;
		printf("c\n");
		state = 0;
		if(pthread_create( &send_thread, NULL, thread_send, NULL) ){
			fprintf(stderr, "Error in creating thread\n");
			return 0;
		}
		if(pthread_create( &receive_thread, NULL, thread_receive, NULL) ){
			fprintf(stderr, "Error in creating receive thread\n");
			return 0;
		}
		sem_wait(&create_sema);
		sem_wait(&create_sema);
		return 0;
	}

	int tcp_close(){
		printf("[tcp_close]\" begin\n");
		sem_wait(&state_sema);
		state = WAITING_FOR_FIN;
		sem_post(&state_sema);
		int i;
		for(i = 0;i<7;i++) {
			printf("sending FIN No.%d\n", i);
			add_send_task("",0,FLAG_FIN, seq, ack, window);
			sleep(1);
			sem_wait(&state_sema);
			if (state == CLOSED) {
				break;
				sem_post(&state_sema);
			}
			sem_post(&state_sema);
		}
		printf("tcp connection closed\n");
	}

	int test_send(unsigned int number) {
		unsigned int len = number * 1000;
		int num_sent = 0;
		int start_seq = seq;
		struct timespec tv;
		tv.tv_sec = 0;
		tv.tv_nsec = 1000000;
		while (num_sent < len) {
			//printf("window = %d\n", window);
			int i;
			int s = seq;
			int start = s;
			int sum = 0;
			for ( i = 0; i < window; i++) {
				add_send_task(test_data, PCKT_LEN, 0, s, ack, 0);
				s+=PCKT_LEN;
				sum += PCKT_LEN;
				if (sum + num_sent >= len){
					break;
				}
			}

			nanosleep(&tv, NULL);
			if (sum == seq - start) {
				window = window +1;
			} else {
				window = window / 2;
			}
			num_sent = seq - start_seq;
			//printf("[test_send] num_sent = %d\n", num_sent);
		}
		return num_sent;
	}

	int sonic_send(char *data, int len) {
		int num_sent = 0;
		int index = 0;
		int start_seq = seq;
		while (num_sent < len) {
			printf("window = %d\n", window);
			int i;
			int j = index;
			int s = seq;
			int l = len - num_sent;
			printf("num_sent = %d, l = %d\n", num_sent, l);
			int start = s;
			int sum = 0;
			for ( i = 0; i < window; i++) {
				if ( PCKT_LEN < l) {
					add_send_task(data+j, PCKT_LEN, 0, s, ack, 0);
					j += PCKT_LEN;
					s += PCKT_LEN;
					sum += PCKT_LEN;
				} else {
					printf("[sonic_send] seq = %d, ack = %d\n", s, ack);
					add_send_task(data+j, l, 0, s, ack, 0);
					j += l;
					s += l;
					sum += l;
					break;//last packet
				}
			}
			printf("[sonic_send] globale seq = %d\n", seq);
			sleep(1);
			if (sum == seq - start) {
				window++;
			} else {
				window = window / 2;
			}
			num_sent = seq - start_seq;
			index = seq-start_seq;
		}	
		return num_sent;
	}
