#ifndef MEMORYMANAGER_H_
#define MEMORYMANAGER_H_

#include "queuemanager.h"
#include "packet.h"

void *memorymanager_function(void *dummyPtr);

int allocatefreepacketbuffers(struct packet_head *queue, int bufferstoallocate);

struct packet *get_freepacket_buffer(void);

int put_freepacket_buffer(struct packet *thispacket);

#endif /*MEMORYMANAGER_H_*/
