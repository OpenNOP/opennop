#ifndef SESSIONCLEANUP_H_
#define SESSIONCLEANUP_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include "session.h"

void sendkeepalive
(__u32 saddr, __u16 source, __u32 seq,
__u32 daddr, __u16 dest, __u32 ack_seq
);

void cleanuplist
(struct session_head *currentlist);

void *cleanup_function(void *data);

#endif /*SESSIONCLEANUP_H_*/
