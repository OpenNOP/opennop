#ifndef QUEUEMANAGER_H_
#define QUEUEMANAGER_H_
#define _GNU_SOURCE

#include <sys/types.h>
#include <linux/types.h>

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "session.h"
#include "packet.h"

extern int DEBUG_QUEUEMANAGER;

int queue_packet(struct packet_head *queue, struct packet *thispacket);

struct packet *dequeue_packet(struct packet_head *queue, int signal);

u_int32_t move_queued_packets(struct packet_head *fromqueue,
		struct packet_head *toqueue);

#endif /*QUEUEMANAGER_H_*/
