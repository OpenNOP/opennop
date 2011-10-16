#include <string.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include "compression.h"
#include "bcl/lz.h"
#include "tcpoptions.h"

/*
 * Compresses the TCP data of an SKB.
 */
unsigned int tcp_compress(__u8 *ippacket, __u8 *lzbuffer, __u8 *lzfastbuffer){
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	__u16 oldsize = 0, newsize = 0; /* Store old, and new size of the TCP data. */
	__u8 *tcpdata = NULL; /* Starting location for the TCP data. */
	
		if (ippacket != NULL){ // If the skb is NULL abort compression.
			iph = (struct iphdr *)ippacket; // Access ip header.
		
			if ((iph->protocol == IPPROTO_TCP)){ // If this is not a TCP segment abort compression.
				tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
				oldsize = (__u16)(ntohs(iph->tot_len) - iph->ihl*4) - tcph->doff*4;
				tcpdata = (__u8 *)tcph + tcph->doff*4; // Find starting location of the TCP data.
				
					if (oldsize > 0){  // Only compress if there is any data.
						newsize = (oldsize*2);
						
						if (lzbuffer != NULL){ // If the buffer is NULL abort compression.
							
							if (lzfastbuffer != NULL){
								
								//memset(lzfastbuffer,0,(LZFASTBUFFER_PAGES * PAGE_SIZE)); // Zero the working buffer.
								newsize = LZ_CompressFast(tcpdata, lzbuffer, oldsize, (unsigned int *)lzfastbuffer); // Compress the data.
							}
							else{
								
								newsize = LZ_Compress(tcpdata, lzbuffer, oldsize);
							}
						
							if (newsize < oldsize){  // Make sure the compressed data is smaller.
								memmove(tcpdata, lzbuffer, newsize); // Move compressed data to packet.
								//pskb_trim(skb,skb->len - (oldsize - newsize)); // Remove extra space from skb.
								iph->tot_len = htons(ntohs(iph->tot_len) - (oldsize - newsize));// Fix packet length.
		 						__set_tcp_option((__u8 *)iph,31,3,1); // Set compression flag.
		 						tcph->seq = htonl(ntohl(tcph->seq) + 8000); // Increase SEQ number.
							}
							else{ // Compressed data was not smaller than original.
							}
						}
					}
			}
		}
	return 1;
}

/*
 * Decompress the TCP data of an SKB.
 */
unsigned int tcp_decompress(__u8 *ippacket, __u8 *lzbuffer, __u8 *lzfastbuffer){
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	__u16 oldsize = 0, newsize = 0; /* Store old, and new size of the TCP data. */
	__u8 *tcpdata = NULL; /* Starting location for the TCP data. */

		if (ippacket != NULL){ // If the skb is NULL abort compression.
			iph = (struct iphdr *)ippacket; // Access ip header.
		
			if ((iph->protocol == IPPROTO_TCP)){ // If this is not a TCP segment abort compression.
				tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl); // Access tcp header.
				oldsize = (__u16)(ntohs(iph->tot_len) - iph->ihl*4) - tcph->doff*4;
				
				tcpdata = (__u8 *)tcph + tcph->doff*4; // Find starting location of the TCP data.
				
				if (oldsize > 0){
						
					if (lzbuffer != NULL){ // If the buffer is NULL abort compression.
						newsize = LZ_Uncompress(tcpdata, lzbuffer, oldsize); // Decompress the TCP data.
						memmove(tcpdata, lzbuffer, newsize); // Move decompressed data to packet.
						iph->tot_len = htons(ntohs(iph->tot_len) + (newsize - oldsize));// Fix packet length.
		 				__set_tcp_option((__u8 *)iph,31,3,0); // Set compression flag to 0.
		 				tcph->seq = htonl(ntohl(tcph->seq) - 8000); // Decrease SEQ number.
		 				return 1;
					}
				}
			}
		}
	return 0;
}
