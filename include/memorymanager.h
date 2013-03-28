#ifndef MEMORYMANAGER_H_
#define MEMORYMANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> // for multi-threading
#include <stdbool.h>

#include <sys/types.h>
#include <linux/types.h>

#include "queuemanager.h"
#include "packet.h"

void *memorymanager_function(void *dummyPtr);

int allocatefreepacketbuffers(struct packet_head *queue, int bufferstoallocate);

u_int32_t movepacketbuffers(struct packet_head *fromqueue,
		struct packet_head *toqueue);

int put_freepacket_buffer(struct packet *thispacket);
#endif /*MEMORYMANAGER_H_*/
