#ifndef WORKER_H_
#define WORKER_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include "packet.h"

#define MAXWORKERS 255 // Maximum number of workers to process packets.

/* Structure contains the worker thread, queue, and status. */
struct worker
{
    pthread_t t_worker; // Is the thread for this worker.
    __u32 packets; // Number of packets this worker has processed.
    __u8 *lzbuffer; // Buffer used for LZ compression.
    __u8 *lzfastbuffer; // Buffer used for FastLZ compression.
    u_int32_t sessions; // Number of sessions assigned to the worker.
    struct packet_head queue; // Points to the queue for this worker.
    int state;	// Marks this thread as active. 1=running, 0=stopping, -1=stopped.
    pthread_mutex_t lock; // Lock for this worker.
};

extern struct worker workers[MAXWORKERS];
extern unsigned char numworkers;

void *worker_function (void *dummyPtr);

#endif /*WORKER_H_*/
