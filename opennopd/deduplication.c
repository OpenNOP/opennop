#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <linux/types.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include <openssl/sha.h>

#include <db.h>

#include "climanager.h"
#include "logger.h"

struct block{
	char data[128];
};

struct hash{
	char hash[64];
};

struct dedup_metrics{
	int hits;
	int misses;
};

#define	DATABASE "/var/opennop/dedup.db"
#define BLOCKS "blocks"
#define MAXBLOCKS 16

static struct dedup_metrics metrics;

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

	/**
	 * @todo Check file path for free space.
	 *  There needs to be state counter that gets the % available space for /var/opennop.
	 *  	If > 80% free then deduplication will create new blocks.
	 *  	If < 80% free it will only lookup existing blocks.
	 */

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

					key.data = &hashes[i];
					key.size = sizeof(struct hash);

					data.data = tcpdatablock[i].data;
					data.size = sizeof(struct block);

					switch ((*dbp)->put(*dbp, NULL, &key, &data, DB_NOOVERWRITE)){
						case 0:
							metrics.misses++;
							//logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Created new block.\n");
							break;
						case DB_KEYEXIST:
							metrics.hits++;
							//logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Duplicate block.\n");
							break;
						case EINVAL:
							logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Invalid record.\n");
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

struct commandresult cli_show_dedup_stats(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result  = { 0 };
    char msg[MAX_BUFFER_SIZE] = { 0 };
    char message[LOGSZ];

    sprintf(msg,"Dedup Hits: %i\n",metrics.hits);
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Dedup Misses: %i\n",metrics.misses);
    cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
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

	/**
	 * Initialize dedup metrics
	 */
	metrics.hits = 0;
	metrics.misses = 0;

	register_command(NULL, "show dedup stats", cli_show_dedup_stats, false, false);


	return 0;
}




