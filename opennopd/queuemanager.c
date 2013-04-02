#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h> // for multi-threading
#include <stdint.h> // Sharwan Joram:  uint32_t etc./ are defined in this
//#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue //Not used Justin Yaple: 3-28-2013
#include "queuemanager.h"
#include "packet.h"
#include "worker.h"
#include "logger.h"

int DEBUG_QUEUEMANAGER = false;

int queue_packet(struct packet_head *queue, struct packet *thispacket) {
	/* Lets add the  packet to a queue. */

	if (thispacket != NULL) {
		pthread_mutex_lock(&queue->lock); // Grab lock on queue.

		if (queue->qlen == 0) { // Check if any packets are in the queue.
			queue->next = thispacket; // Queue next will point to the new packet.
			queue->prev = thispacket; // Queue prev will point to the new packet.
		} else {
			thispacket->prev = queue->prev; // Packet prev will point at the last packet in the queue.
			thispacket->prev->next = thispacket;
			queue->prev = thispacket; // Make this new packet the last packet in the queue.
		}

		queue->qlen += 1; // Need to increase the packet count in this queue.
		pthread_cond_signal(&queue->signal);
		pthread_mutex_unlock(&queue->lock); // Lose lock on queue.
		return 0;
	}
	return -1; // error packet was null
}

/*
 * Gets the next packet from a queue.
 * This can sleep if signal parameter is true = 1 not false = 0.
 */
struct packet *dequeue_packet(struct packet_head *queue, int signal) {
	struct packet *thispacket = NULL;
	char message[LOGSZ];

	/* Lets get the next packet from the queue. */
	pthread_mutex_lock(&queue->lock); // Grab lock on the queue.

	if ((queue->qlen == 0) && (signal == true)) { // If there is no work wait for some.
		pthread_cond_wait(&queue->signal, &queue->lock);
	}

	if (DEBUG_QUEUEMANAGER == true) {
		sprintf(message, "Queue Manager: Queue has %d packets!\n", queue->qlen);
		logger(LOG_INFO, message);
	}

	if (queue->next != NULL) { // Make sure there is work.

		thispacket = queue->next; // Get the next packet in the queue.
		queue->next = thispacket->next; // Move the next packet forward in the queue.
		queue->qlen -= 1; // Need to decrease the packet cound on this queue.
		thispacket->next = NULL;
		thispacket->prev = NULL;
	} else {

		sprintf(message, "Queue Manager: Fatal - Queue missing packet!\n");
		logger(LOG_INFO, message);
	}

	pthread_mutex_unlock(&queue->lock); // Lose lock on the queue.

	return thispacket;
}

/*
 * This function moves all packet buffers from one queue to another.
 * Returns how many were moved.
 */
u_int32_t move_queued_packets(struct packet_head *fromqueue,
		struct packet_head *toqueue) {
	struct packet *start; // Points to the first packet of the list.
	struct packet *end; // Points to the last packet of the list.
	u_int32_t qlen; // How many buffers are being moved.

	/*
	 * Get the first and last buffers in the source queue
	 * and remove all the packet buffers from the source queue.
	 */
	pthread_mutex_lock(&fromqueue->lock);
	start = fromqueue->next;
	end = fromqueue->prev;
	qlen = fromqueue->qlen;
	fromqueue->next = NULL;
	fromqueue->prev = NULL;
	fromqueue->qlen = 0;
	pthread_mutex_unlock(&fromqueue->lock);

	/*
	 * Add those buffers to the destination queue.
	 */
	pthread_mutex_lock(&toqueue->lock);
	if (toqueue->next == NULL) {
		toqueue->next = start;
		toqueue->prev = end;
		toqueue->qlen = qlen;
	} else {
		start->prev = toqueue->prev;
		toqueue->prev->next = start;
		toqueue->prev = end;
		toqueue->qlen += qlen;
	}
	pthread_mutex_unlock(&toqueue->lock);

	return qlen;
}
