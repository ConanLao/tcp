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

typedef struct {
  uint16_t src_port;
  uint16_t dst_port;
  uint32_t seq;
  uint32_t ack;
  uint8_t  data_offset;  // 4 bits
  uint8_t  flags;
  uint16_t window_size;
  uint16_t checksum;
  uint16_t urgent_p;
  uint8_t data[0];
} tcp_header_t;
