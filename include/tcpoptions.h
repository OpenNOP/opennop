#ifndef TCPOPTIONS_H_
#define TCPOPTIONS_H_
#define _GNU_SOURCE

#include <linux/types.h>

extern int DEBUG_TCPOPTIONS;

__u8 optlen(const __u8 *opt, __u8 offset);
__u64 __get_tcp_option(__u8 *ippacket, __u8 tcpoptnum);
int __set_tcp_option(__u8 *ippacket, unsigned int tcpoptnum,
unsigned int tcpoptlen, u_int64_t tcpoptdata);
 
#endif /*TCPOPTIONS_H_*/
