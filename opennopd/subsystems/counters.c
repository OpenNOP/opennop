#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for sleep function
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <linux/types.h>

#include "opennopd.h"
#include "logger.h"
#include "fetcher.h"
#include "worker.h"
#include "counters.h"
#include "climanager.h"

int DEBUG_COUNTERS = true;
int DEBUG_COUNTERS_REGISTER = false;

/*
 * Time in seconds before updating the counters.
 */
int UPDATECOUNTERSTIMER = 5;
struct counter_head counters;

void *counters_function(void *dummyPtr) {
	char message[LOGSZ];

	while (servicestate >= RUNNING) {
		sleep(UPDATECOUNTERSTIMER); // Sleeping for a few seconds.

		/*
		 * Here is the new method.
		 */
		execute_counters();
	}

	/*
	 * Thread is ending.
	 */
	sprintf(message, "Counters: Shutdown thread.\n");
	logger(LOG_INFO, message);
	return NULL;
}

/*
 * This function calculates pps or bps.
 */
int calculate_ppsbps(__u32 previouscount, __u32 currentcount) {
	int ppsbps;
	__u32 scale;

	/*
	 * Make sure previous metric is less then current.  Did they roll?
	 */
	if (previouscount < currentcount) {
		ppsbps = (currentcount - previouscount) / UPDATECOUNTERSTIMER;
	} else {
		/*
		 * They must have rolled if they are less.
		 * Should we scale them by highest value of __u32?
		 * I don't even know what the highest value of __u32 is!
		 * Can we use sizeof() * 8bit^32???
		 * This rolls the counters half way around so current > previous.
		 */
		scale = (1UL << (sizeof(__u32 ) * 8)) / 2;
		ppsbps = ((currentcount + scale) - (previouscount + scale))
				/ UPDATECOUNTERSTIMER;

	}

	return ppsbps;
}

/*
 * Other modules call this when they have metrics that need to be calculate.
 * They must include the data to where the metric data is stored and a
 * handler function to process the metrics.
 */
int register_counter(t_counterfunction handler, t_counterdata data) {
	struct counter *currentcounter;
	char message[LOGSZ];

	if (DEBUG_COUNTERS_REGISTER == true) {
		sprintf(message, "Counters: Enter register_counter!");
		logger(LOG_INFO, message);
	};

	/**
	 * @todo:
	 * We might need some detection logic here to ensure the same counter is not being registered twice.
	 * For now we will assume that it will only be registered once.
	 */

	currentcounter = allocate_counter(); //Allocate a new counter instance.
	currentcounter->handler = handler; //Assign the handler function passed from the caller.
	currentcounter->data = data; //Assign the data passed by the caller.

	/*
	 * Lock the counters list while we update this.
	 */
	pthread_mutex_lock(&counters.lock);

	if (counters.next == NULL) {
		counters.next = currentcounter;
		counters.prev = currentcounter;
	} else {
		currentcounter->prev = counters.prev;
		counters.prev->next = currentcounter;
		counters.prev = currentcounter;
	}

	pthread_mutex_unlock(&counters.lock);

	if (DEBUG_COUNTERS_REGISTER == true) {
		sprintf(message, "Counters: Exit register_counter!");
		logger(LOG_INFO, message);
	};
	return 1;
}

struct counter* allocate_counter() {
	struct counter *newcounter = (struct counter *) malloc(
			sizeof(struct counter));
	if (newcounter == NULL) {
		fprintf(stdout, "Could not allocate memory... \n");
		exit(1);
	}
	newcounter->next = NULL;
	newcounter->prev = NULL;
	newcounter->handler = NULL;
	newcounter->data = NULL;
	return newcounter;
}

void execute_counters() {
	struct counter *currentcounter;
	/*
	 * We loop through the list and execute each counter handle
	 * passing its data pointer too.
	 */
	currentcounter = counters.next;
	pthread_mutex_lock(&counters.lock);
	while (currentcounter != NULL) {
		(currentcounter->handler)(currentcounter->data);//Call the handler function passing its data.
		currentcounter = currentcounter->next;
	}
	pthread_mutex_unlock(&counters.lock);
}

