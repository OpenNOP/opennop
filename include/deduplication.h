#ifndef DEDUPLICATION_H_
#define DEDUPLICATION_H_

int deduplicate(__u8 *ippacket, DB **dbp);
int dbp_initialize(DB *dbp);

#endif
