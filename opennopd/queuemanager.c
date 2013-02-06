#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h> // for multi-threading

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "queuemanager.h"
#include "packet.h"
#include "worker.h"
#include "logger.h"

int DEBUG_QUEUEMANAGER = false;

int queue_packet(struct packet_head *queue, struct packet *thispacket)
{
    /* Lets add the  packet to a queue. */
    pthread_mutex_lock(&queue->lock); // Grab lock on queue.

    if (queue->qlen == 0)
    { // Check if any packets are in the queue.
        queue->next = thispacket; // Queue next will point to the new packet.
        queue->prev = thispacket; // Queue prev will point to the new packet.
    }
    else
    {
        thispacket->prev = queue->prev; // Packet prev will point at the last packet in the queue.
        thispacket->prev->next = thispacket;
        queue->prev = thispacket; // Make this new packet the last packet in the queue.
    }

    queue->qlen += 1; // Need to increase the packet count in this queue.
    pthread_cond_signal(&queue->signal);
    pthread_mutex_unlock(&queue->lock); // Lose lock on queue.

    return 0;
}

/*
 * Gets the next packet from a queue.
 * This can sleep if signal parameter is not NULL.
 */
struct packet *dequeue_packet(struct packet_head *queue, int signal)
{
    struct packet *thispacket = NULL;
    char message [LOGSZ];

    /* Lets get the next packet from the queue. */
    pthread_mutex_lock(&queue->lock);  // Grab lock on the queue.

    if ((queue->qlen == 0) && (signal == true))
    { // If there is no work wait for some.
        pthread_cond_wait(&queue->signal, &queue->lock);
    }

    if (queue->next != NULL)
    { // Make sure there is work.

        if (DEBUG_QUEUEMANAGER == true)
        {
            sprintf(message, "Queue Manager: Queue has %d packets!\n", queue->qlen);
            logger(LOG_INFO, message);
        }

        thispacket = queue->next; // Get the next packet in the queue.
        queue->next = thispacket->next; // Move the next packet forward in the queue.
        queue->qlen -= 1; // Need to decrease the packet cound on this queue.
        thispacket->next = NULL;
        thispacket->prev = NULL;
    }

    pthread_mutex_unlock(&queue->lock);  // Lose lock on the queue.

    return thispacket;
}
