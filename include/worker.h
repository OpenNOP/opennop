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
struct workercounters {
	/*
	 * number of packets processed.
	 * used for pps calculation. can roll.
	 * should increment at end of main loop after each packet is finished processing.
	 */
	__u32 packets;
	__u32 packetsprevious;
	__u32 pps; // Where the calculated pps are stored.

	/*
	 * number of bytes entering process.
	 * used for bps calculation. can roll.
	 * should increment as beginning of main loop after packet is received from queue.
	 */
	__u32 bytesin;
	__u32 bytesinprevious;
	__u32 bpsin; // Where the calculated bps are stored.
	__u32 bytesout;
	__u32 bytesoutprevious;
	__u32 bpsout; // Where the calculated bps are stored.

	/*
	 * Stores when the counters were last updated.
	 */
	struct timeval updated;

	/*
	 * When something reads/writes to this counter it should use the lock.
	 */
	pthread_mutex_t lock;
};

/*
 * This structure is a member of the worker structure.
 * It contains the thread for optimization or de-optimization of packets,
 * the counters for this thread, the threads packet queue
 * and the buffer used for QuickLZ.
 */
struct processor {
	pthread_t t_processor;
	struct workercounters metrics;
	struct packet_head queue;
	__u8 *lzbuffer; // Buffer used for QuickLZ.
};

/* Structure contains the worker threads, queue, and status. */
struct worker {
	struct processor optimization; //Thread that will do all optimizations(input).  Coming from LAN.
	struct processor deoptimization; //Thread that will undo optimizations(output).  Coming from WAN.
	u_int32_t sessions; // Number of sessions assigned to the worker.
	int state; // Marks this thread as active. 1=running, 0=stopping, -1=stopped.
	pthread_mutex_t lock; // Lock for this worker when adding sessions.
};

void *optimization_thread(void *dummyPtr);
void *deoptimization_thread(void *dummyPtr);
unsigned char get_workers(void);
void set_workers(unsigned char desirednumworkers);
u_int32_t get_worker_sessions(int i);
void create_worker(int i);
void rejoin_worker(int i);
void initialize_worker_processor(struct processor *thisprocessor);
void joining_worker_processor(struct processor *thisprocessor);
void set_worker_state_running(struct worker *thisworker);
void set_worker_state_stopped(struct worker *thisworker);
void increment_worker_sessions(int i);
void decrement_worker_sessions(int i);
int optimize_packet(__u8 queue, struct packet *thispacket);
int deoptimize_packet(__u8 queue, struct packet *thispacket);
void shutdown_workers();
int cli_show_workers(int client_fd, char **parameters, int numparameters);
void counter_updateworkermetrics(t_counterdata metric);
struct session *closingsession(struct tcphdr *tcph, struct session *thissession);

#endif /*WORKER_H_*/
