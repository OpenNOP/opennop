#ifndef COMPRESSION_H_
#define COMPRESSION_H_
#define _GNU_SOURCE

#include <linux/types.h>

unsigned int tcp_compress(__u8 *ippacket, __u8 *lzbuffer, __u8 *lzfastbuffer);
unsigned int tcp_decompress(__u8 *ippacket, __u8 *lzbuffer, __u8 *lzfastbuffer);

#endif /*COMPRESSION_H_*/
