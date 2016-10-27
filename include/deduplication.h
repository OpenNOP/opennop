#ifndef DEDUPLICATION_H_
#define DEDUPLICATION_H_

#define DEDUP_BLOCKS "blocks.db"

int init_deduplication();
int deduplicate_V1(__u8 *data, __u32 length, DB **dbp, __u8 *lzbuffer);
int deduplicate_tcp_data_V1(__u8 *ippacket, __u8 *lzbuffer, struct session *thissession);
int create_dedup_blocks(__u8 *ippacket, DB **dbp, char *neighborID);
int rehydration(__u8 *ippacket, DB **dbp);
int dbp_initialize(DB **dbp, char *database);

#endif
