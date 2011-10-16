#ifndef SESSIONMANAGER_H_
#define SESSIONMANAGER_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include "session.h"

#define SESSIONBUCKETS 65536 // Number of buckets in the hash table for sessoin.

extern struct session_head sessiontable[SESSIONBUCKETS]; // Setup the session hashtable.

extern int DEBUG_SESSIONMANAGER_INSERT;
extern int DEBUG_SESSIONMANAGER_GET;
extern int DEBUG_SESSIONMANAGER_REMOVE;

__u16 sessionhash(__u32 largerIP, __u16 largerIPPort, 
__u32 smallerIP, __u16 smallerIPPort);
void freemem(struct session_head *currentlist);
struct session *insertsession(__u32 largerIP, __u16 largerIPPort,
				__u32 smallerIP, __u16 smallerIPPort);
struct session *getsession(__u32 largerIP, __u16 largerIPPort, 
__u32 smallerIP, __u16 smallerIPPort);
void clearsession(struct session *currentsession);
void sort_sockets(__u32 *largerIP, __u16 *largerIPPort,
					__u32 *smallerIP, __u16 *smallerIPPort,
					__u32 saddr, __u16 source, 
					__u32 daddr, __u16 dest);

#endif /*SESSIONMANAGER_H_*/
