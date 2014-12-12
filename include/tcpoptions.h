#ifndef TCPOPTIONS_H_
#define TCPOPTIONS_H_
#define _GNU_SOURCE

#include <linux/types.h>

#define ONOP "ONOP"

struct nodhdr {
	__u8  tot_len;		// combine length of header + data
	__u8  idlen:3,		// length of id in bytes (should be 1-4)
	      hdr_len:5;	// length  of the header (additional data can be appended inside the header as header data)
	__u8  id;
	__u8  *hdrdata;		// optional
	__u8  *data;
};

struct tcp_opt_nod{
	__u8  option_num;
	__u8  option_len;
	struct nodhdr nodh;
};

__u8 optlen(const __u8 *opt, __u8 offset);
__u64 __get_tcp_option(__u8 *ippacket, __u8 tcpoptnum);
int __set_tcp_option(__u8 *ippacket, unsigned int tcpoptnum,
unsigned int tcpoptlen, u_int64_t tcpoptdata);
struct nodhdr *get_nod_header(__u8 *ippacket, const char *id);
struct nodhdr *set_nod_header(__u8 *ippacket, const char *id);
void set_nod_header_data(__u8 *ippacket, const char *id, __u8 *header_data, __u8 header_data_length);
__u8 *get_nod_header_data(__u8 *ippacket, const char *id);
#endif /*TCPOPTIONS_H_*/
