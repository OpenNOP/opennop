#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/types.h>
#include <linux/types.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include <openssl/sha.h>

#include <db.h>

#include "logger.h"

struct block{
	char data[128];
};

struct hash{
	char hash[64];
};

#define	DATABASE "/var/opennop/dedup.db"
#define BLOCKS "blocks"
#define MAXBLOCKS 16

/** @brief Calculate hash values
 *
 * This function is just a test to determine performance impact of deduplication by hashing each block of data in a packet.
 *
 * @param *ippacket [in] The IP packet containing data to be hashed.
 * @return int 0 = success -1 = failed
 */
int deduplicate(__u8 *ippacket, DB **dbp){
	//* @todo Don't copy the data here.  No point.
	//struct block blocks[12] = {0}; // 12 * 128 byte blocks = 1536 large enough for any standard size IP packet.
	struct hash hashes[MAXBLOCKS]; // hash values for each block
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	struct block *tcpdatablock = NULL;
	__u16 datasize = 0;
	__u8  numblocks = 0;
	int i = 0;
	DBT key, data;
	int ret = 0;

	if (ippacket != NULL) {
		iph = (struct iphdr *) ippacket; // Access ip header.

		if (iph->protocol == IPPROTO_TCP) { // If this is not a TCP segment abort compression.
			tcph = (struct tcphdr *) (((u_int32_t *) ippacket) + iph->ihl);

			datasize = (__u16)(ntohs(iph->tot_len) - iph->ihl * 4) - tcph->doff * 4;
			tcpdatablock = (__u8 *) tcph + tcph->doff * 4; // Find starting location of the TCP data.

			numblocks = datasize / 128;

			if((datasize % 128) != 0){
				numblocks = numblocks + 1;
			}

			memset(&hashes, 0, sizeof(struct hash)*MAXBLOCKS);

			if(numblocks < MAXBLOCKS){

				for(i=0;i<numblocks;i++){
					SHA512((unsigned char*)tcpdatablock[i].data, 128, (unsigned char *)&hashes[i]);

					memset(&key, 0, sizeof(key));
					memset(&data, 0, sizeof(data));

					memcpy(&key.data, &hashes[i], sizeof(struct hash));
					key.size = sizeof(struct hash);

					memcpy(&data.data, &tcpdatablock[i].data, sizeof(struct block));
					data.size = sizeof(struct block);

					switch (ret = (*dbp)->put(*dbp, NULL, &key, &data, DB_NOOVERWRITE)){
						case 0:
							break;
						case DB_KEYEXIST:
							logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Duplicate block.\n");
							break;
						default:
							logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Unknown error!\n");
							exit(1);
					}
				}
			}
		}
	}

	return 0;
}

int dbp_initialize(DB **dbp){
	int ret = 0;

	if ((ret = db_create(dbp, NULL, 0)) != 0) {
		logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Error creating database pointer.\n");
		exit (1);
	}

	if ((ret = (*dbp)->open(*dbp, NULL, DATABASE, BLOCKS, DB_BTREE, DB_CREATE | DB_THREAD, 0664)) != 0) {
		(*dbp)->err(*dbp, ret, "%s", DATABASE);
		logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Opening database failed.\n");
		exit (1);
	}
	logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Opening database success.\n");

	return 0;
}




