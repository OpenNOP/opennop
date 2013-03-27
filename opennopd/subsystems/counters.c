#include <stdio.h>
#include <unistd.h> // for sleep function
#include <stdbool.h>

#include <linux/types.h>

#include "opennopd.h"
#include "logger.h"
#include "fetcher.h"
#include "worker.h"
#include "counters.h"

int DEBUG_COUNTERS = true;

/*
 * Time in seconds before updating the counters.
 */
int UPDATECOUNTERSTIMER = 5;

void *counters_function(void *dummyPtr) {
	int i;
	char message[LOGSZ];

	/*
	 * Storage for the previous thread metrics.
	 */
	struct counters prevoptimizationmetrics[MAXWORKERS];
	struct counters prevdeoptimizationmetrics[MAXWORKERS];
	struct counters prevfetchermetrics;

	/*
	 * Storage for the current thread metrics.
	 */
	struct counters optimizationmetrics[MAXWORKERS];
	struct counters deoptimizationmetrics[MAXWORKERS];
	struct counters fetchermetrics;

	/*
	 * Initialize previous thread storage.
	 */
	prevfetchermetrics = thefetcher.metrics;
	for (i = 0; i < numworkers; i++) {
		prevoptimizationmetrics[i] = workers[i].optimization.metrics;
		prevdeoptimizationmetrics[i] = workers[i].deoptimization.metrics;
	}

	while (servicestate >= STOPPING) {
		sleep(UPDATECOUNTERSTIMER); // Sleeping for a few seconds.

		/*
		 * Get current fetcher metrics,
		 * calculate the pps metrics,
		 * and save them.
		 */
		fetchermetrics = thefetcher.metrics;
		thefetcher.metrics.pps = calculate_ppsbps(prevfetchermetrics.packets,
				fetchermetrics.packets);
		thefetcher.metrics.bpsin = calculate_ppsbps(prevfetchermetrics.bytesin,
				fetchermetrics.bytesin);
		prevfetchermetrics = fetchermetrics;

		if ((DEBUG_COUNTERS == true) && (thefetcher.metrics.pps != 0)
				&& (thefetcher.metrics.bpsin != 0)) {
			sprintf(message, "Counters: Fetcher: %u pps\n",
					thefetcher.metrics.pps);
			logger(LOG_INFO, message);
			sprintf(message, "Counters: Fetcher: %u bps\n",
					thefetcher.metrics.bpsin);
			logger(LOG_INFO, message);
		}

		for (i = 0; i < numworkers; i++) {
			/*
			 * We get the current metrics for each thread.
			 */
			optimizationmetrics[i] = workers[i].optimization.metrics;
			deoptimizationmetrics[i] = workers[i].deoptimization.metrics;

			/*
			 * Calculate the pps metrics,
			 * and save them.
			 */
			workers[i].optimization.metrics.pps = calculate_ppsbps(
					prevoptimizationmetrics[i].packets,
					optimizationmetrics[i].packets);
			workers[i].optimization.metrics.bpsin = calculate_ppsbps(
					prevoptimizationmetrics[i].bytesin,
					optimizationmetrics[i].bytesin);
			workers[i].optimization.metrics.bpsout = calculate_ppsbps(
					prevoptimizationmetrics[i].bytesout,
					optimizationmetrics[i].bytesout);

			workers[i].deoptimization.metrics.pps = calculate_ppsbps(
					prevdeoptimizationmetrics[i].packets,
					deoptimizationmetrics[i].packets);
			workers[i].deoptimization.metrics.bpsin = calculate_ppsbps(
					prevdeoptimizationmetrics[i].bytesin,
					deoptimizationmetrics[i].bytesin);
			workers[i].deoptimization.metrics.bpsout = calculate_ppsbps(
					prevdeoptimizationmetrics[i].bytesout,
					deoptimizationmetrics[i].bytesout);

			/*
			 * Last we move the current metrics into the previous metrics.
			 */
			prevoptimizationmetrics[i] = optimizationmetrics[i];
			prevdeoptimizationmetrics[i] = deoptimizationmetrics[i];

		}

		if (DEBUG_COUNTERS == true) {

			for (i = 0; i < numworkers; i++) {

				if ((workers[i].optimization.metrics.pps != 0)
						&& (workers[i].optimization.metrics.bpsin != 0)
						&& (workers[i].optimization.metrics.bpsout != 0)
						&& (workers[i].deoptimization.metrics.pps != 0)
						&& (workers[i].deoptimization.metrics.bpsin != 0)
						&& (workers[i].deoptimization.metrics.bpsout != 0)) {
					sprintf(message, "Counters: Optimization: %u pps\n",
							workers[i].optimization.metrics.pps);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Optimization: %u bps in\n",
							workers[i].optimization.metrics.bpsin);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Optimization: %u bps out\n",
							workers[i].optimization.metrics.bpsout);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Deoptimization: %u pps\n",
							workers[i].deoptimization.metrics.pps);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Deoptimization: %u bps in\n",
							workers[i].deoptimization.metrics.bpsin);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Deoptimization: %u bps out\n",
							workers[i].deoptimization.metrics.bpsout);
					logger(LOG_INFO, message);
				}
			}
		}

	}

	/*
	 * Thread is ending.
	 */
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
