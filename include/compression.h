#ifndef COMPRESSION_H_
#define COMPRESSION_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include "quicklz.h"

unsigned int tcp_compress(__u8 *ippacket, __u8 *lzbuffer, qlz_state_compress *state_compress);
unsigned int tcp_decompress(__u8 *ippacket, __u8 *lzbuffer, qlz_state_decompress *state_decompress);

#endif /*COMPRESSION_H_*/
