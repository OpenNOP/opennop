#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <linux/types.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include <openssl/sha.h>

#include "logger.h"

struct block{
	char data[128];
};

struct hash{
	char hash[64];
};

/** @brief Calculate hash values
 *
 * This function is just a test to determine performance impact of deduplication by hashing each block of data in a packet.
 *
 * @param *ippacket [in] The IP packet containing data to be hashed.
 * @return int 0 = success -1 = failed
 */
int deduplicate(__u8 *ippacket){
	//* @todo Don't copy the data here.  No point.
	//struct block blocks[12] = {0}; // 12 * 128 byte blocks = 1536 large enough for any standard size IP packet.
	struct hash hashes[12]; // hash values for each block
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	struct block *data = NULL;
	__u16 datasize = 0;
	__u8  numblocks = 0;
	int i = 0;

	if (ippacket != NULL) {
		iph = (struct iphdr *) ippacket; // Access ip header.

		if (iph->protocol == IPPROTO_TCP) { // If this is not a TCP segment abort compression.
					tcph = (struct tcphdr *) (((u_int32_t *) ippacket) + iph->ihl);

					datasize = (__u16)(ntohs(iph->tot_len) - iph->ihl * 4) - tcph->doff * 4;
					data = (__u8 *) tcph + tcph->doff * 4; // Find starting location of the TCP data.

					numblocks = datasize / 128;

					if((datasize % 128) != 0){
						numblocks = numblocks + 1;
					}

					memset(&hashes, 12, sizeof(struct hash));

					if(numblocks < 12){

						for(i=0;i<numblocks;i++){
							SHA512((unsigned char*)data[i].data, 128, (unsigned char *)&hashes[i]);
						}
					}
		}

	}




	return 0;
}




