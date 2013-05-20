#ifndef FETCHER_H_
#define FETCHER_H_
#define _GNU_SOURCE

#include <pthread.h>

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "counters.h"

struct fetcher {
	pthread_t t_fetcher;
	struct counters metrics;
	int state; // Marks this thread as active. 1=running, 0=stopping, -1=stopped.
	pthread_mutex_t lock; // Lock for the fetcher when changing state.
};

extern struct fetcher thefetcher;

int fetcher_callback(struct nfq_q_handle *hq, struct nfgenmsg *nfmsg,
		struct nfq_data *nfa, void *data);

void *fetcher_function(void *dummyPtr);
void fetcher_graceful_exit();

#endif /*FETCHER_H_*/
