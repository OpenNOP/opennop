#ifndef TCPOPTIONS_H_
#define TCPOPTIONS_H_
#define _GNU_SOURCE

#include <linux/types.h>

#define ONOP "ONOP"

struct nodhdr {
	__u8  length;
	__u8  idlen:2,
	      len:6;
	__u8  iddata;
	__u8  *data;
};

__u8 optlen(const __u8 *opt, __u8 offset);
__u64 __get_tcp_option(__u8 *ippacket, __u8 tcpoptnum);
int __set_tcp_option(__u8 *ippacket, unsigned int tcpoptnum,
unsigned int tcpoptlen, u_int64_t tcpoptdata);
struct nodhdr *set_nod_header(__u8 *ippacket, const char *id);
struct nodhdr *get_nod_header(__u8 *ippacket, const char *id);
#endif /*TCPOPTIONS_H_*/
