#ifndef FETCHER_H_
#define FETCHER_H_
#define _GNU_SOURCE

#include <pthread.h>

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue
#include "counters.h"

struct fetchercounters {
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

	/*
	 * Stores when the counters were last updated.
	 */
	struct timeval updated;

	/*
	 * When something reads/writes to this counter it should use the lock.
	 */
	pthread_mutex_t lock;
};

struct fetcher {
	pthread_t t_fetcher;
	struct fetchercounters metrics;
	int state; // Marks this thread as active. 1=running, 0=stopping, -1=stopped.
	pthread_mutex_t lock; // Lock for the fetcher when changing state.
};

int fetcher_callback(struct nfq_q_handle *hq, struct nfgenmsg *nfmsg,
		struct nfq_data *nfa, void *data);

void *fetcher_function(void *dummyPtr);
void fetcher_graceful_exit();
void create_fetcher();
void rejoin_fetcher();
struct commandresult cli_show_fetcher(int client_fd, char **parameters, int numparameters, void *data);
void counter_updatefetchermetrics(t_counterdata data);

#endif /*FETCHER_H_*/
