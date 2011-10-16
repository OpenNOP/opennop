#include <stdlib.h>
#include <string.h>
#include <pthread.h> // for multi-threading

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "queuemanager.h"
#include "packet.h"
#include "worker.h"

int queue_packet(struct nfq_q_handle *hq, u_int32_t id, int ret, __u8 *originalpacket, struct session *thissession){
struct packet *newpacket = NULL;
__u8 queuenum = 0;

	newpacket = calloc(1,sizeof(struct packet)); // Allocate space for a new packet.
	newpacket->head = NULL;
	newpacket->next = NULL;
	newpacket->prev = NULL;
	newpacket->hq = hq; // Save the queue handle.
	newpacket->id = id; // Save this packets id.
	memmove(newpacket->data, originalpacket, ret); // Save the packet.
	
	/*
	 * Need check what queue the packet should go to.
	 * For now lets just use queue 0.
	 */
	queuenum = thissession->queue;
	
	newpacket->head = &workers[queuenum].queue; // Set the packet head to its queue.
	
	/* Lets add the new packet to the queue. */
	pthread_mutex_lock(&workers[queuenum].queue.lock); // Grab lock on queue.
			
	if (workers[queuenum].queue.qlen == 0){ // Check if any packets are in the queue.
		workers[queuenum].queue.next = newpacket; // Queue next will point to the new packet.
		workers[queuenum].queue.prev = newpacket; // Queue prev will point to the new packet.
	}
	else{
		newpacket->prev = workers[queuenum].queue.prev; // Packet prev will point at the last packet in the queue.
		newpacket->prev->next = newpacket;
		workers[queuenum].queue.prev = newpacket; // Make this new packet the last packet in the queue.
	}
		
	workers[queuenum].queue.qlen += 1; // Need to increase the packet count in this queue.
	pthread_cond_signal(&workers[queuenum].signal);
	pthread_mutex_unlock(&workers[queuenum].queue.lock); // Lose lock on queue.	


	return 0;
}
