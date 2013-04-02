#ifndef WORKER_H_
#define WORKER_H_
#define _GNU_SOURCE

#include <stdint.h>
#include <pthread.h>

#include <sys/types.h>

#include <linux/types.h>

#include "packet.h"
#include "counters.h"


#define MAXWORKERS 255 // Maximum number of workers to process packets.

/*
 * This structure is a member of the worker structure.
 * It contains the thread for optimization or de-optimization of packets,
 * the counters for this thread, the threads packet queue
 * and the buffer used for QuickLZ.
 */
struct processor
{
	pthread_t t_processor;
	struct counters metrics;
	struct packet_head queue;
	__u8 *lzbuffer; // Buffer used for QuickLZ.
};

/* Structure contains the worker threads, queue, and status. */
struct worker
{
    struct processor optimization; //Thread that will do all optimizations(input).  Coming from LAN.
    struct processor deoptimization; //Thread that will undo optimizations(output).  Coming from WAN.
    u_int32_t sessions; // Number of sessions assigned to the worker.
    int state;	// Marks this thread as active. 1=running, 0=stopping, -1=stopped.
    pthread_mutex_t lock; // Lock for this worker when adding sessions.
};

extern struct worker workers[MAXWORKERS];
//extern unsigned char numworkers;

void *optimization_function (void *dummyPtr);
void *deoptimization_function (void *dummyPtr);
unsigned char get_workers(void);
void set_workers(unsigned char desirednumworkers);

#endif /*WORKER_H_*/
