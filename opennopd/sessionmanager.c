#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h> // for multi-threading
#include <linux/types.h>

#include <arpa/inet.h>

#include "sessionmanager.h"
#include "opennopd.h"
#include "logger.h"
#include "worker.h"
#include "session.h"
#include "clicommands.h"

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

	for (i = 0; i < get_workers(); i++) {

		if (DEBUG_SESSIONMANAGER_INSERT == true) {
			sprintf(message, "Session Manager: Queue #%d has %d sessions.\n",
					i, get_worker_sessions(i));
			logger(LOG_INFO, message);
		}

		if (get_worker_sessions(queuenum) > get_worker_sessions(i)) {

			if (i < get_workers()) {
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
		newsession->client = NULL;
		newsession->server = NULL;
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
		increment_worker_sessions(queuenum);

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
struct session *clearsession(struct session *currentsession) {
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
		decrement_worker_sessions(currentsession->queue);
		free(currentsession);
		currentsession = NULL;
	}
	return currentsession;
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

void initialize_sessiontable() {
	int i;
	for (i = 0; i < SESSIONBUCKETS; i++) { // Initialize all the slots in the hashtable to NULL.
		sessiontable[i].next = NULL;
		sessiontable[i].prev = NULL;
	}
}

void clear_sessiontable() {
	int i;
	char message[LOGSZ];

	for (i = 0; i < SESSIONBUCKETS; i++) { // Initialize all the slots in the hashtable to NULL.
		if (sessiontable[i].next != NULL) {
			freemem(&sessiontable[i]);
			sprintf(message, "Exiting: Freeing sessiontable %d!\n", i);
			logger(LOG_INFO, message);
		}

	}
}

struct session_head *getsessionhead(int i) {
	return &sessiontable[i];
}

struct commandresult cli_show_sessionss(int client_fd, char **parameters, int numparameters, void *data) {
	struct session *currentsession = NULL;
	struct commandresult result = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };
	int i;
	char temp[20];
	char col1[10];
	char col2[17];
	char col3[14];
	char col4[17];
	char col5[14];
	char col6[14];
	char end[3];

	sprintf(
			msg,
			"--------------------------------------------------------------------------------------\n");
	cli_send_feedback(client_fd, msg);
	sprintf(
			msg,
			"|  Index  |   Client IP    | Client Port |    Server IP   | Server Port | Optimizing |\n");
	cli_send_feedback(client_fd, msg);
	sprintf(
			msg,
			"--------------------------------------------------------------------------------------\n");
	cli_send_feedback(client_fd, msg);

	/*
	 * Check each index of the session table for any sessions.
	 */
	for (i = 0; i < SESSIONBUCKETS; i++) {

		/*
		 * Skip any index of the sessiontable that has no sessions.
		 */
		if (sessiontable[i].next != NULL) {
			currentsession = sessiontable[i].next;

			/*
			 * Work through all sessions in that index and print them out.
			 */
			while (currentsession != NULL) {

				/**
				 * @todo:
				 * This will only show the session if we know what IPs are client & server.
				 * Its possible we wont know that if a session opening is not witnessed
				 * by OpenNOP.  OpenNOP has a "recover" mechanism that allows the session
				 * to be optimized if it detects another OpenNOP appliance.
				 * https://sourceforge.net/p/opennop/bugs/16/
				 */
				if ((currentsession->client != NULL) && (currentsession->server
						!= NULL)) {
					strcpy(msg, "");
					sprintf(col1, "|  %-7i", i);
					strcat(msg, col1);
					inet_ntop(AF_INET, currentsession->client, temp,
							INET_ADDRSTRLEN);
					sprintf(col2, "| %-15s", temp);
					strcat(msg, col2);
					sprintf(col3, "|   %-10i", ntohs(
							currentsession->largerIPPort));
					strcat(msg, col3);
					inet_ntop(AF_INET, currentsession->server, temp,
							INET_ADDRSTRLEN);
					sprintf(col4, "| %-15s", temp);
					strcat(msg, col4);
					sprintf(col5, "|   %-10i", ntohs(
							currentsession->smallerIPPort));
					strcat(msg, col5);

					if ((((currentsession->largerIPAccelerator == localID)
							|| (currentsession->smallerIPAccelerator == localID))
							&& ((currentsession->largerIPAccelerator != 0)
									&& (currentsession->smallerIPAccelerator
											!= 0))
							&& (currentsession->largerIPAccelerator
									!= currentsession->smallerIPAccelerator))) {
						sprintf(col6, "|     Yes    ");
					} else {
						sprintf(col6, "|     No     ");
					}
					strcat(msg, col6);
					sprintf(end, "|\n");
					strcat(msg, end);
					cli_send_feedback(client_fd, msg);

					currentsession = currentsession->next;
				}
			}
		}

	}

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

int updateseq(__u32 largerIP, struct iphdr *iph, struct tcphdr *tcph,
		struct session *thissession) {

	if ((largerIP != 0) && (iph != NULL) && (tcph != NULL) && (thissession != NULL)) {

		if (iph->saddr == largerIP) { // See what IP this is coming from.

			if (ntohl(tcph->seq) != (thissession->largerIPseq - 1)) {
				thissession->largerIPStartSEQ = ntohl(tcph->seq);
			}
		} else {

			if (ntohl(tcph->seq) != (thissession->smallerIPseq - 1)) {
				thissession->smallerIPStartSEQ = ntohl(tcph->seq);
			}
		}
		return 0; // Everything OK.
	}

	return -1; // Had a problem!
}

int sourceisclient(__u32 largerIP, struct iphdr *iph, struct session *thissession) {

	if ((largerIP != 0) && (iph != NULL) && (thissession != NULL)) {

		if (iph->saddr == largerIP) { // See what IP this is coming from.
			thissession->client = &thissession->largerIP;
			thissession->server = &thissession->smallerIP;
		} else {
			thissession->client = &thissession->smallerIP;
			thissession->server = &thissession->largerIP;
		}
		return 0;// Everything  OK.
	}
	return -1;// Had a problem.
}

int saveacceleratorid(__u32 largerIP, __u32 acceleratorID, struct iphdr *iph, struct session *thissession) {

	if ((largerIP != 0) && (iph != NULL) && (thissession != NULL)){

		if (iph->saddr == largerIP)
		{ // Set the Accelerator for this source.
			thissession->largerIPAccelerator = acceleratorID;
		}
		else
		{
			thissession->smallerIPAccelerator = acceleratorID;
		}
		return 0;// Everything  OK.
	}
	return -1;// Had a problem.
}
