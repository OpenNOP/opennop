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
 * TODO: This whole module could use a re-write already.
 * A better solution might be to  have a linked list of pointers to struct counters.
 * The linked list would be internal only and let other modules register their counters here.
 * This process would loop through them all to update the stats based on the UPDATECOUNTERSTIMER.
 * This would be consistent with no special processing for worker/optimization/de-optimization or fether.
 * In the future this method would allow other modules to use the same counters structure.
 */

/*
 * Time in seconds before updating the counters.
 */
int UPDATECOUNTERSTIMER = 5;
struct counter_head newcounters;

void *counters_function(void *dummyPtr) {
	int i;
	char message[LOGSZ];

	/*
	 * Storage for the previous thread metrics.
	 */
	//struct counters prevoptimizationmetrics[MAXWORKERS];
	//struct counters prevdeoptimizationmetrics[MAXWORKERS];
	//struct counters prevfetchermetrics;

	/*
	 * Storage for the current thread metrics.
	 */
	//struct counters optimizationmetrics[MAXWORKERS];
	//struct counters deoptimizationmetrics[MAXWORKERS];
	//struct counters fetchermetrics;

	/*
	 * Initialize new counters list.
	 */
	//newcounters.next = NULL;
	//newcounters.prev = NULL;
	//pthread_mutex_init(&newcounters.lock, NULL);

	/*
	 * Initialize previous thread storage.
	 */
	//prevfetchermetrics = get_fetcher_counters();
	//for (i = 0; i < get_workers(); i++) {
	//	prevoptimizationmetrics[i] = get_optimization_counters(i);
	//	prevdeoptimizationmetrics[i] = get_deoptimization_counters(i);
	//}

	while (servicestate >= RUNNING) {
		sleep(UPDATECOUNTERSTIMER); // Sleeping for a few seconds.


		/*
		 * Here is the new method.
		 */
		execute_counters();

		/*
		 * Get current fetcher metrics,
		 * calculate the pps metrics,
		 * and save them.
		 */
		//fetchermetrics = get_fetcher_counters();
		//set_fetcher_pps(calculate_ppsbps(prevfetchermetrics.packets,
		//		fetchermetrics.packets));
		//set_fetcher_bpsin(calculate_ppsbps(prevfetchermetrics.bytesin,
		//		fetchermetrics.bytesin));
		/*
		 * Don't have any use for bpsout here really.
		 */
		//set_fetcher_bpsout(calculate_ppsbps(prevfetchermetrics.bytesout,
		//				fetchermetrics.bytesout));
		//prevfetchermetrics = fetchermetrics;

		//if ((DEBUG_COUNTERS == true) && (get_fetcher_counters().pps != 0)
		//		&& (get_fetcher_counters().bpsin != 0)) {
		//	sprintf(message, "Counters: Fetcher: %u pps\n",
		//			get_fetcher_counters().pps);
		//	logger(LOG_INFO, message);
		//	sprintf(message, "Counters: Fetcher: %u bps\n",
		//			get_fetcher_counters().bpsin);
		//	logger(LOG_INFO, message);
		//}

		//for (i = 0; i < get_workers(); i++) {
			/*
			 * We get the current metrics for each thread.
			 */
			//optimizationmetrics[i] = get_optimization_counters(i);
			//deoptimizationmetrics[i] = get_deoptimization_counters(i);

			/*
			 * Calculate the pps metrics,
			 * and save them.
			 */
			/*
			set_optimization_pps(i, calculate_ppsbps(
					prevoptimizationmetrics[i].packets,
					optimizationmetrics[i].packets));
			set_optimization_bpsin(i, calculate_ppsbps(
					prevoptimizationmetrics[i].bytesin,
					optimizationmetrics[i].bytesin));
			set_optimization_bpsout(i, calculate_ppsbps(
					prevoptimizationmetrics[i].bytesout,
					optimizationmetrics[i].bytesout));

			set_deoptimization_pps(i, calculate_ppsbps(
					prevdeoptimizationmetrics[i].packets,
					deoptimizationmetrics[i].packets));
			set_deoptimization_bpsin(i, calculate_ppsbps(
					prevdeoptimizationmetrics[i].bytesin,
					deoptimizationmetrics[i].bytesin));
			set_deoptimization_bpsout(i, calculate_ppsbps(
					prevdeoptimizationmetrics[i].bytesout,
					deoptimizationmetrics[i].bytesout));
			*/

			/*
			 * Last we move the current metrics into the previous metrics.
			 */
			//prevoptimizationmetrics[i] = optimizationmetrics[i];
			//prevdeoptimizationmetrics[i] = deoptimizationmetrics[i];

		//}

		/*
		if (DEBUG_COUNTERS == true) {

			for (i = 0; i < get_workers(); i++) {

				if ((get_optimization_counters(i).pps != 0)
						&& (get_optimization_counters(i).bpsin != 0)
						&& (get_optimization_counters(i).bpsout != 0)
						&& (get_deoptimization_counters(i).pps != 0)
						&& (get_deoptimization_counters(i).bpsin != 0)
						&& (get_deoptimization_counters(i).bpsout != 0)) {
					sprintf(message, "Counters: Optimization: %u pps\n",
							get_optimization_counters(i).pps);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Optimization: %u bps in\n",
							get_optimization_counters(i).bpsin);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Optimization: %u bps out\n",
							get_optimization_counters(i).bpsout);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Deoptimization: %u pps\n",
							get_deoptimization_counters(i).pps);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Deoptimization: %u bps in\n",
							get_deoptimization_counters(i).bpsin);
					logger(LOG_INFO, message);
					sprintf(message, "Counters: Deoptimization: %u bps out\n",
							get_deoptimization_counters(i).bpsout);
					logger(LOG_INFO, message);
				}
			}
		}
		*/

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

struct counters get_counters(struct counters *thiscounter) {
	struct counters thecounters;
	pthread_mutex_lock(&thiscounter->lock);
	thecounters = *thiscounter;
	pthread_mutex_unlock(&thiscounter->lock);
	return thecounters;
}

void set_pps(struct counters *thiscounter, __u32 count) {
	pthread_mutex_lock(&thiscounter->lock);
	thiscounter->pps = count;
	pthread_mutex_unlock(&thiscounter->lock);
}

void set_bpsin(struct counters *thiscounter, __u32 count) {
	pthread_mutex_lock(&thiscounter->lock);
	thiscounter->bpsin = count;
	pthread_mutex_unlock(&thiscounter->lock);
}

void set_bpsout(struct counters *thiscounter, __u32 count) {
	pthread_mutex_lock(&thiscounter->lock);
	thiscounter->bpsout = count;
	pthread_mutex_unlock(&thiscounter->lock);
}

/*
 * Other modules call this when they have metrics that need to be calculate.
 * They must include the data to where the metric data is stored and a
 * handler function to process the metrics.
 */
int register_counter(t_counterfunction handler, t_counterdata data) {
	struct counter *currentcounter;
	char message[LOGSZ];

	if (DEBUG_COUNTERS_REGISTER == true){
		sprintf(message, "Counters: Enter register_counter!");
		logger(LOG_INFO, message);
	};

	/*
	 * TODO:
	 * We might need some detection logic here to ensure the same counter is not being registered twice.
	 * For now we will assume that it will only be registered once.
	 */

	currentcounter = allocate_counter(); //Allocate a new counter instance.
	currentcounter->handler = handler; //Assign the handler function passed from the caller.
	currentcounter->data = data; //Assign the data passed by the caller.

	/*
	 * Lock the counters list while we update this.
	 */
	pthread_mutex_lock(&newcounters.lock);

	if(newcounters.next == NULL) {
		newcounters.next = currentcounter;
		newcounters.prev = currentcounter;
	}else{
		currentcounter->prev = newcounters.prev;
		newcounters.prev->next = currentcounter;
		newcounters.prev = currentcounter;
	}

	pthread_mutex_unlock(&newcounters.lock);

	if (DEBUG_COUNTERS_REGISTER == true){
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
	currentcounter = newcounters.next;
	pthread_mutex_lock(&newcounters.lock);
	while(currentcounter != NULL) {
		(currentcounter->handler)(currentcounter->data);//Call the handler function passing its data.
		currentcounter = currentcounter->next;
	}
	pthread_mutex_unlock(&newcounters.lock);
}

