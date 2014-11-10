// The packet length
#define PCKT_LEN 8192

//TCP flags
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
