#ifndef QUEUEMANAGER_H_
#define QUEUEMANAGER_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "session.h"

int queue_packet(struct nfq_q_handle *hq, u_int32_t id, int ret, __u8 *originalpacket, struct session *thissession);

#endif /*QUEUEMANAGER_H_*/
