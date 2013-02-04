#include <stdlib.h>
#include <pthread.h>

#include "eventmanager.h"
#include "opennopd.h"

pthread_cond_t g_eventmanager_signal;
struct event_head g_eventmanager_queue;

void *eventmanager_function (void *dummyPtr){
	
	while (servicestate >= STOPPING) {
		
		/* Lets get the next event from the queue. */
		pthread_mutex_lock(&g_eventmanager_queue.lock);  // Grab lock on the queue.
		
		if (g_eventmanager_queue.qlen == 0){ // If there is no work wait for some.
			pthread_cond_wait(&g_eventmanager_signal, &g_eventmanager_queue.lock);
		}
		
		/*
		 * Logic to grab events from the event queue goes here.
		 */
		
		pthread_mutex_unlock(&g_eventmanager_queue.lock);  // Lose lock on the queue.
		
		/*
		 * Logic to process events needs to go here.
		 */
	}
	return NULL;
}
