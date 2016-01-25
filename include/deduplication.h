#ifndef DEDUPLICATION_H_
#define DEDUPLICATION_H_

#define DEDUP_BLOCKS "blocks.db"

int deduplicate(__u8 *data, __u32 length, DB **dbp);
int create_dedup_blocks(__u8 *ippacket, DB **dbp);
int rehydration(__u8 *ippacket, DB **dbp);
int dbp_initialize(DB **dbp, char *database);

#endif
