#include <stdio.h>
#include <unistd.h> // for sleep function
#include <stdbool.h>

#include <linux/types.h>

#include "opennopd.h"
#include "logger.h"
#include "worker.h"
#include "counters.h"

int DEBUG_COUNTERS = true;

/*
 * Time in seconds before updating the counters.
 */
int UPDATECOUNTERSTIMER = 5;

/*
 * Store the performance statistics.
 */
__u32 fetcherbps;
__u32 fetcherbps;
__u32 optimizationpps[MAXWORKERS];
__u32 deoptimizationpps[MAXWORKERS];
__u32 optimizationbps[MAXWORKERS];
__u32 deoptimizationbps[MAXWORKERS];

void *counters_function(void *dummyPtr) {
	__u32 ppsbps; //Temp storage for calculating pps & bps.
	int i;
	char message[LOGSZ];

	/*
	 * Storage for the previous thread metrics.
	 */
	struct counters prevoptimizationmetrics[MAXWORKERS];
	struct counters prevdeoptimizationmetrics[MAXWORKERS];
	struct counters prevfetcherpmetrics;

	/*
	 * Storage for the current thread metrics.
	 */
	struct counters optimizationmetrics[MAXWORKERS];
	struct counters deoptimizationmetrics[MAXWORKERS];
	struct counters fetchermetrics;

	/*
	 * Initialize previous thread storage.
	 */
	for (i = 0; i < numworkers; i++) {
		optimizationmetrics[i] = workers[i].optimization.metrics;
		deoptimizationmetrics[i] = workers[i].deoptimization.metrics;
	}

	while (servicestate >= STOPPING) {
		sleep(UPDATECOUNTERSTIMER); // Sleeping for seconds.

		/*
		 * We get the current metrics for each thread.
		 */
		for (i = 0; i < numworkers; i++) {
			optimizationmetrics[i] = workers[i].optimization.metrics;
			deoptimizationmetrics[i] = workers[i].deoptimization.metrics;
		}

		/*
		 * Next we compare them to the saved metrics.
		 */
		for (i = 0; i < numworkers; i++) {

			/*
			 * Make sure previous optimization metrics are less then current.  Did they roll?
			 */
			ppsbps = calculate_ppsbps(prevoptimizationmetrics[i].packets,optimizationmetrics[i].packets);

			/*
			 * Save the optimization performance statistics.
			 */
			optimizationpps[i] = ppsbps;


			ppsbps = calculate_ppsbps(prevdeoptimizationmetrics[i].packets,deoptimizationmetrics[i].packets);

			/*
			 * Save the optimization performance statistics.
			 */
			deoptimizationpps[i] = ppsbps;

		}

		/*
		 * Last we move the current metrics into the previous metrics.
		 */
		for (i = 0; i < numworkers; i++) {
			prevoptimizationmetrics[i] = optimizationmetrics[i];
			prevdeoptimizationmetrics[i] = deoptimizationmetrics[i];
		}

		if (DEBUG_COUNTERS == true) {

			for (i = 0; i < numworkers; i++) {
				sprintf(message, "Counters: Optimization: %u pps\n",
						optimizationpps[i]);
				logger(LOG_INFO, message);
				sprintf(message, "Counters: Deoptimization: %u pps\n",
						deoptimizationpps[i]);
				logger(LOG_INFO, message);
			}
		}

	}

	/*
	 * Thread is ending.
	 */
	return NULL;
}

/*
 * This function c
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
		scale = (1UL << (sizeof(__u32)*8))/2;
		ppsbps = ((currentcount + scale) - (previouscount + scale))
				/ UPDATECOUNTERSTIMER;

	}

	return ppsbps;
}
