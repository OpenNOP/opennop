#ifndef QUEUEMANAGER_H_
#define QUEUEMANAGER_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "session.h"
#include "packet.h"

extern int DEBUG_QUEUEMANAGER;

//int queue_packet(struct nfq_q_handle *hq, u_int32_t id, int ret, __u8 *originalpacket, struct session *thissession);
int queue_packet(struct packet_head *queue, struct packet *thispacket);
struct packet *dequeue_packet(struct packet_head *queue, int signal);

#endif /*QUEUEMANAGER_H_*/
