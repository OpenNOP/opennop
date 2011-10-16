#ifndef FETCHER_H_
#define FETCHER_H_
#define _GNU_SOURCE

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

extern int DEBUG_FETCHER;

int fetcher_callback(struct nfq_q_handle *hq, struct nfgenmsg *nfmsg,
			  struct nfq_data *nfa, void *data);

void *fetcher_function (void *dummyPtr);

#endif /*FETCHER_H_*/
