#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h> // for multi-threading
#include <linux/types.h>

#include "sessionmanager.h"
#include "opennopd.h"
#include "logger.h"
#include "worker.h"
#include "session.h"

struct session_head sessiontable[SESSIONBUCKETS]; // Setup the session hashtable.

int DEBUG_SESSIONMANAGER_INSERT = false;
int DEBUG_SESSIONMANAGER_GET = false;
int DEBUG_SESSIONMANAGER_REMOVE = false;

/*
 * Calculates the hash of a session provided the IP addresses, and ports.
 */
__u16 sessionhash(__u32 largerIP, __u16 largerIPPort, __u32 smallerIP,
		__u16 smallerIPPort) {
	__u16 hash1 = 0, hash2 = 0, hash3 = 0, hash4 = 0, hash = 0;

	hash1 = (largerIP ^ smallerIP) >> 16;
	hash2 = (largerIP ^ smallerIP);
	hash3 = hash1 ^ hash2;
	hash4 = largerIPPort ^ smallerIPPort;
	hash = hash3 ^ hash4;
	return hash;
}

/* 
 * This function frees all memory dynamically allocated for the session linked list. 
 */
void freemem(struct session_head *currentlist) {
	struct session *currentsession = NULL;

	if (currentlist->next != NULL) {
		currentsession = currentlist->next;

		while (currentlist->next != NULL) {

			printf("Freeing session!\n");
			if (currentsession->prev != NULL) { // Is there a previous session.
				free(currentsession->prev); // Free the previous session.
				currentsession->prev = NULL; // Assign the previous session NULL.
			}

			if (currentsession->next != NULL) { // Check if there are more sessions.
				currentsession = currentsession->next; // Advance to the next session.
			} else { // No more sessions.
				free(currentsession); // Free the last session.
				currentsession = NULL; // Set the current session as NULL.
				currentlist->next = NULL; // Assign the list as NULL.
				currentlist->prev = NULL; // Assign the list as NULL.
			}
		}
	}
	return;
}

/* 
 * Inserts a new sessio.n into the sessions linked list.
 * Will either use an empty slot, or create a new session in the list.
 */
struct session *insertsession(__u32 largerIP, __u16 largerIPPort,
		__u32 smallerIP, __u16 smallerIPPort) {
	struct session *newsession = NULL;
	int i;
	__u16 hash = 0;
	__u8 queuenum = 0;
	char message[LOGSZ];

	hash = sessionhash(largerIP, smallerIP, largerIPPort, smallerIPPort);

	/*
	 * What queue will the packets for this session go to?
	 */
	queuenum = 0;

	for (i = 0; i < numworkers; i++) {

		if (DEBUG_SESSIONMANAGER_INSERT == true) {
			sprintf(message, "Session Manager: Queue #%d has %d sessions.\n",
					i, workers[i].sessions);
			logger(LOG_INFO, message);
		}

		if (workers[queuenum].sessions > workers[i].sessions) {

			if (i < numworkers) {
				queuenum = i;
			}
		}
	}

	if (DEBUG_SESSIONMANAGER_INSERT == true) {
		sprintf(message,
				"Session Manager: Assigning session to queue #: %d!\n",
				queuenum);
		logger(LOG_INFO, message);
	}

	newsession = calloc(1, sizeof(struct session)); // Allocate a new session.

	if (newsession != NULL) { // Write data to this new session.
		newsession->head = &sessiontable[hash]; // Pointer to the head of this list.
		newsession->next = NULL;
		newsession->prev = NULL;
		newsession->queue = queuenum;
		newsession->largerIP = largerIP; // Assign values and initialize this session.
		newsession->largerIPPort = largerIPPort;
		newsession->largerIPAccelerator = 0;
		newsession->largerIPseq = 0;
		newsession->smallerIP = smallerIP;
		newsession->smallerIPPort = smallerIPPort;
		newsession->smallerIPseq = 0;
		newsession->smallerIPAccelerator = 0;
		newsession->deadcounter = 0;
		newsession->state = 0;

		/*
		 * Increase the counter for number of sessions assigned to this worker.
		 */
		pthread_mutex_lock(&workers[queuenum].lock); // Grab lock on worker.
		workers[queuenum].sessions += 1;
		pthread_mutex_unlock(&workers[queuenum].lock); // Lose lock on worker.

		/* 
		 * Lets add the new session to the session bucket.
		 */
		pthread_mutex_lock(&sessiontable[hash].lock); // Grab lock on the session bucket.

		if (DEBUG_SESSIONMANAGER_INSERT == true) {
			sprintf(message,
					"Session Manager: Assigning session to bucket #: %u!\n",
					hash);
			logger(LOG_INFO, message);
		}

		if (sessiontable[hash].qlen == 0) { // Check if any session are in this bucket.
			sessiontable[hash].next = newsession; // Session Head next will point to the new session.
			sessiontable[hash].prev = newsession; // Session Head prev will point to the new session.
		} else {
			newsession->prev = sessiontable[hash].prev; // Session prev will point at the last packet in the session bucket.
			newsession->prev->next = newsession;
			sessiontable[hash].prev = newsession; // Make this new session the last session in the session bucket.
		}

		sessiontable[hash].qlen += 1; // Need to increase the session count in this session bucket.	

		if (DEBUG_SESSIONMANAGER_INSERT == true) {
			sprintf(
					message,
					"Session Manager: There are %u sessions in this bucket now.\n",
					sessiontable[hash].qlen);
			logger(LOG_INFO, message);
		}

		pthread_mutex_unlock(&sessiontable[hash].lock); // Lose lock on session bucket.

		return newsession;
	} else {
		return NULL; // Failed to assign memory for newsession.
	}
}

/*
 * Gets the sessionindex for the TCP session.
 * Returns NULL if hits the end of the list without a match.
 */
struct session *getsession(__u32 largerIP, __u16 largerIPPort, __u32 smallerIP,
		__u16 smallerIPPort) {
	struct session *currentsession = NULL;
	__u16 hash = 0;
	char message[LOGSZ];

	hash = sessionhash(largerIP, smallerIP, largerIPPort, smallerIPPort);

	if (DEBUG_SESSIONMANAGER_GET == true) {
		sprintf(message,
				"Session Manager: Seaching for session in bucket #: %u!\n",
				hash);
		logger(LOG_INFO, message);
	}
	if (sessiontable[hash].next != NULL) { // Testing for sessions in the list.
		currentsession = sessiontable[hash].next; // There is at least one session in the list.
	} else { // No sessions were in this list.

		if (DEBUG_SESSIONMANAGER_GET == true) {
			sprintf(message, "Session Manager: No session was found.\n");
			logger(LOG_INFO, message);
		}
		return NULL;
	}

	while (currentsession != NULL) { // Looking for session.

		if ((currentsession->largerIP == largerIP) && // Check if session matches.
				(currentsession->largerIPPort == largerIPPort)
				&& (currentsession->smallerIP == smallerIP)
				&& (currentsession->smallerIPPort == smallerIPPort)) {

			if (DEBUG_SESSIONMANAGER_GET == true) {
				sprintf(message, "Session Manager: A session was found.\n");
				logger(LOG_INFO, message);
			}
			return currentsession; // Session matched so save session.
		} else {

			if (currentsession->next != NULL) { // Not a match move to next session.
				currentsession = currentsession->next;
			} else { // No more sessions so no session exists.

				if (DEBUG_SESSIONMANAGER_GET == true) {
					sprintf(message, "Session Manager: No session was found.\n");
					logger(LOG_INFO, message);
				}
				return NULL;
			}
		}
	}

	// Something went very bad if this runs.
	if (DEBUG_SESSIONMANAGER_GET == true) {
		sprintf(message, "Session Manager: FATAL! No session was found.\n");
		logger(LOG_INFO, message);
	}
	return NULL;
}

/*
 * Resets the sessionindex, and session. 
 */
void clearsession(struct session *currentsession) {
	__u16 hash = 0;
	char message[LOGSZ];

	if (currentsession != NULL) { // Make sure session is not NULL.

		if (DEBUG_SESSIONMANAGER_REMOVE == true) {
			hash = sessionhash(currentsession->largerIP,
					currentsession->smallerIP, currentsession->largerIPPort,
					currentsession->smallerIPPort);
			sprintf(message,
					"Session Manager: Removing session from bucket #: %u!\n",
					hash);
			logger(LOG_INFO, message);
		}

		pthread_mutex_lock(&currentsession->head->lock); // Grab lock on the session bucket.

		if ((currentsession->next == NULL) && (currentsession->prev == NULL)) { // This should be the only session.
			currentsession->head->next = NULL;
			currentsession->head->prev = NULL;
		}

		if ((currentsession->next != NULL) && (currentsession->prev == NULL)) { // This is the first session.
			currentsession->next->prev = NULL; // Set the previous session as the last.
			currentsession->head->next = currentsession->next; // Update the first session in head to the next.
		}

		if ((currentsession->next == NULL) && (currentsession->prev != NULL)) { // This is the last session.
			currentsession->prev->next = NULL; // Set the previous session as the last.
			currentsession->head->prev = currentsession->prev; // Update the last session in head to the previous.
		}

		if ((currentsession->next != NULL) && (currentsession->prev != NULL)) { // This is in the middle of the list.
			currentsession->prev->next = currentsession->next; // Sets the previous session next to the next session.
			currentsession->next->prev = currentsession->prev; // Sets the next session previous to the previous session.
		}

		currentsession->head->qlen -= 1; // Need to increase the session count in this session bucket.	

		pthread_mutex_unlock(&currentsession->head->lock); // Lose lock on session bucket.

		if (DEBUG_SESSIONMANAGER_REMOVE == true) {
			sprintf(
					message,
					"Session Manager: There are %u sessions in this bucket now.\n",
					currentsession->head->qlen);
			logger(LOG_INFO, message);
		}

		/*
		 * Decrease the counter for number of sessions assigned to this worker.
		 */
		pthread_mutex_lock(&workers[currentsession->queue].lock); // Grab lock on worker.
		workers[currentsession->queue].sessions -= 1;
		pthread_mutex_unlock(&workers[currentsession->queue].lock); // Lose lock on worker.


		free(currentsession);
	}
	return;
}

/*
 * Puts sockets in order.
 */
void sort_sockets(__u32 *largerIP, __u16 *largerIPPort, __u32 *smallerIP,
		__u16 *smallerIPPort, __u32 saddr, __u16 source, __u32 daddr,
		__u16 dest) {
	if (saddr > daddr) { // Using source IP address as largerIP.
		*largerIP = saddr;
		*largerIPPort = source;
		*smallerIP = daddr;
		*smallerIPPort = dest;
	} else { // Using destination IP address as largerIP.
		*largerIP = daddr;
		*largerIPPort = dest;
		*smallerIP = saddr;
		*smallerIPPort = source;
	}
}
