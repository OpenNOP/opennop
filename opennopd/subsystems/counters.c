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
	for (i = 0; i < get_workers(); i++) {
		prevoptimizationmetrics[i] = get_optimization_counters(i);
		prevdeoptimizationmetrics[i] = get_deoptimization_counters(i);
	}

	register_command("show counters", cli_show_counters);

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

		for (i = 0; i < get_workers(); i++) {
			/*
			 * We get the current metrics for each thread.
			 */
			optimizationmetrics[i] = get_optimization_counters(i);
			deoptimizationmetrics[i] = get_deoptimization_counters(i);

			/*
			 * Calculate the pps metrics,
			 * and save them.
			 */
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

			/*
			 * Last we move the current metrics into the previous metrics.
			 */
			prevoptimizationmetrics[i] = optimizationmetrics[i];
			prevdeoptimizationmetrics[i] = deoptimizationmetrics[i];

		}

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

int cli_show_counters() {
	int i;
	__u32 ppsbps;
	__u32 total_optimization_pps = 0, total_optimization_bpsin = 0,
			total_optimization_bpsout = 0;
	__u32 total_deoptimization_pps = 0, total_deoptimization_bpsin = 0,
			total_deoptimization_bpsout = 0;
	char msg[MAX_BUFFER_SIZE] = { 0 };
	char message[LOGSZ];
	char col1[11];
	char col2[9];
	char col3[11];
	char col4[12];
	char col5[9];
	char col6[11];
	char col7[12];
	char col8[3];

	if (DEBUG_COUNTERS == true) {
		sprintf(message, "Counters: Showing counters");
		logger(LOG_INFO, message);
	}

	sprintf(msg,
			"---------------------------------------------------------------------\n");
	cli_send_feedback(msg);
	sprintf(msg,
			"|  5 sec  |        optimization        |       deoptimization       |\n");
	cli_send_feedback(msg);
	sprintf(msg,
			"---------------------------------------------------------------------\n");
	cli_send_feedback(msg);
	sprintf(msg,
			"| Worker# |  pps  |  bpsin  |  bpsout  |  pps  |  bpsin  |  bpsout  |\n");
	cli_send_feedback(msg);
	sprintf(msg,
			"---------------------------------------------------------------------\n");
	cli_send_feedback(msg);

	strcpy(msg, "");

	for (i = 0; i < get_workers(); i++) {

		sprintf(col1, "| %-8i", i);
		strcat(msg, col1);

		ppsbps = get_optimization_counters(i).pps;
		total_optimization_pps += ppsbps;
		sprintf(col2, "| %-6u", ppsbps);
		strcat(msg, col2);

		ppsbps = get_optimization_counters(i).bpsin;
		total_optimization_bpsin += ppsbps;
		sprintf(col3, "| %-8u", ppsbps);
		strcat(msg, col3);

		ppsbps = get_optimization_counters(i).bpsout;
		total_optimization_bpsout += ppsbps;
		sprintf(col4, "| %-9u", ppsbps);
		strcat(msg, col4);

		ppsbps = get_deoptimization_counters(i).pps;
		total_deoptimization_pps += ppsbps;
		sprintf(col5, "| %-6u", ppsbps);
		strcat(msg, col5);

		ppsbps = get_deoptimization_counters(i).bpsin;
		total_deoptimization_bpsin += ppsbps;
		sprintf(col6, "| %-8u", ppsbps);
		strcat(msg, col6);

		ppsbps = get_deoptimization_counters(i).bpsout;
		total_deoptimization_bpsout += ppsbps;
		sprintf(col7, "| %-9u", ppsbps);
		strcat(msg, col7);

		sprintf(col8, "|\n");
		strcat(msg, col8);

		cli_send_feedback(msg);
	}
		sprintf(msg,
				"---------------------------------------------------------------------\n");
		cli_send_feedback(msg);

		sprintf(msg, "|  total  | %-6u| %-8u| %-9u| %-6u| %-8u| %-9u|\n",
				total_optimization_pps, total_optimization_bpsin,
				total_optimization_bpsout, total_deoptimization_pps,
				total_deoptimization_bpsin, total_deoptimization_bpsout);
		cli_send_feedback(msg);

		sprintf(msg,
				"---------------------------------------------------------------------\n");
		cli_send_feedback(msg);

		return 0;
	}
