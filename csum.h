#ifndef CSUM_H_
#define CSUM_H_
#define _GNU_SOURCE

unsigned short tcp_sum_calc(unsigned short len_tcp, unsigned short *src_addr, unsigned short *dest_addr, unsigned short *buff);
unsigned short ip_sum_calc(unsigned short len_ip_header, unsigned short *buff);
void checksum(unsigned char *packet);

#endif /*CSUM_H_*/
