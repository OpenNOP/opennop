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
#include "deduplication.h"

struct block{
	char data[128];
};

struct hash{
	char hash[64];
};

struct dedup_metrics{
	int hits;
	int misses;
	int newblocks;
	int collisions;
};


#define	DEDUP_ENVPATH "/var/opennop/db"
#define DEDUP_MAXBLOCKS 16

static int deduplication = true;
static struct dedup_metrics metrics;
static DB_ENV	*dedup_db_environment;

int calculate_sha512(unsigned char *data, int length ,unsigned char *result){
	SHA512_CTX ctx;

	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
	SHA512_Init(&ctx);
	SHA512_Update(&ctx, (unsigned char*)data, length);
	SHA512_Final(result, &ctx);

	return 0;
}


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
	struct hash hashes[DEDUP_MAXBLOCKS]; // hash values for each block
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

	if ((ippacket != NULL) && (deduplication == true)) {
		iph = (struct iphdr *) ippacket; // Access ip header.

		if (iph->protocol == IPPROTO_TCP) { // If this is not a TCP segment abort compression.
			tcph = (struct tcphdr *) (((u_int32_t *) ippacket) + iph->ihl);

			datasize = (__u16)(ntohs(iph->tot_len) - iph->ihl * 4) - tcph->doff * 4;
			tcpdatablock = (__u8 *) tcph + tcph->doff * 4; // Find starting location of the TCP data.

			numblocks = datasize / 128;

			if((datasize % 128) != 0){
				numblocks = numblocks + 1;
			}

			memset(&hashes, 0, sizeof(struct hash)*DEDUP_MAXBLOCKS);

			if(numblocks < DEDUP_MAXBLOCKS){

				for(i=0;i<numblocks;i++){
					SHA512((unsigned char *)tcpdatablock[i].data, 128, (unsigned char *)&hashes[i]);
					// This method works but the above is simpler.
					// It might be better to allocate one permanent CTX for each worker.
					//calculate_sha512((unsigned char *)tcpdatablock[i].data, 128, (unsigned char *)&hashes[i]);
					//binary_dump("[DEDUP HASH]", (unsigned char *)&hashes[i], sizeof(struct hash));
					//binary_dump("[DEDUP DATA]", (unsigned char *)tcpdatablock[i].data, 128);

					memset(&key, 0, sizeof(key));
					memset(&data, 0, sizeof(data));

					key.data = (unsigned char *)&hashes[i];
					key.size = sizeof(struct hash);

					data.data = (unsigned char *)tcpdatablock[i].data;
					data.size = sizeof(struct block);

					// Check if key & data exist in the database.
					switch ((*dbp)->get(*dbp, NULL, &key, &data, DB_GET_BOTH)){

						// Key and data don't exist or doesn't match.
						case DB_NOTFOUND:
							metrics.misses++;

							// Try inserting the key & data into the database.
							switch ((*dbp)->put(*dbp, NULL, &key, &data, DB_NOOVERWRITE)){

								// New key & data saved.
								case 0:
									metrics.newblocks++;
									//logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Created new block.\n");
									break;

								// An existing key exists with different data! (Collision needs resolved!)
								// We hope this never happens!
								case DB_KEYEXIST:
									metrics.collisions++;
									/**
									 * @todo: Need to write a process for block collision resolution.
									 */
									//logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Duplicate block.\n");
									break;

								// Failed to create key & data record in database.
								case EINVAL:
									logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Invalid record.\n");
									break;

								case ENOSPC:
									logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Btree depth exceeded.\n");
									break;

								// Something bad happened.
								default:
									logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Unknown error!\n");
									exit(1);
							}
							break;

						// Key and data pair found in database.
						case 0:
							metrics.hits++;
							break;

						// Invalid query?
						case EINVAL:
							logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Invalid query.\n");
							break;

						// Unknown error in query.
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

int rehydration(__u8 *ippacket, DB **dbp){

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
    sprintf(msg,"Dedup New Blocks: %i\n",metrics.newblocks);
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Dedup Collisions: %i\n",metrics.collisions);
    cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

struct commandresult cli_deduplication_enable(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };
	deduplication = true;
	sprintf(msg, "deduplication enabled\n");
	cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

struct commandresult cli_deduplication_disable(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };
	deduplication = false;
	sprintf(msg, "deduplication disabled\n");
	cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

struct commandresult cli_show_deduplication(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result  = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };

	if (deduplication == true) {
		sprintf(msg, "deduplication enabled\n");
	} else {
		sprintf(msg, "deduplication disabled\n");
	}
	cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

int dbp_initialize(DB **dbp, char *database){
	int ret = 0;

	if ((ret = db_create(dbp, dedup_db_environment, 0)) != 0) {
		logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Error creating database pointer.\n");
		exit (1);
	}

	if ((ret = (*dbp)->open(*dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		(*dbp)->err(*dbp, ret, "%s", database);
		logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Opening database failed.\n");
		exit (1);
	}
	logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[DEDUP] Opening database success.\n");

	return 0;
}

int init_deduplication(){
	/**
	 * Initialize dedup metrics
	 */
	metrics.hits = 0;
	metrics.misses = 0;
	metrics.newblocks = 0;

    register_command(NULL, "deduplication enable", cli_deduplication_enable, false, false);
    register_command(NULL, "deduplication disable", cli_deduplication_disable, false, false);
    register_command(NULL, "show deduplication", cli_show_deduplication, false, false);
	register_command(NULL, "show dedup stats", cli_show_dedup_stats, false, false);

	if(db_env_create(&dedup_db_environment, 0) != 0){
		logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[BDB] Failed creating environment.\n");
		exit(1);
	}

	if(dedup_db_environment->open(dedup_db_environment, DEDUP_ENVPATH, DB_CREATE | DB_INIT_MPOOL | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN | DB_RECOVER, 0) != 0 ){
		logger2(LOGGING_DEBUG,LOGGING_DEBUG,"[BDB] Failed opening environment.\n");
		exit(1);
	}

	return 0;
}

