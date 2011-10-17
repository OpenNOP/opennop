#ifndef EVENTMANAGER_H_
#define EVENTMANAGER_H_

struct event_head {
	struct event *next; /* Points to the first event of the list. */
	struct event *prev; /* Points to the last event of the list. */
	u_int32_t qlen; // Total number of event in the list.
	pthread_mutex_t lock; // Lock for this session bucket.
};

struct event{
	struct event_head *head; // Points to the head of this list.
	struct event *next; /* Points to the first event of the list. */
	struct event *prev; /* Points to the last event of the list. */
	int event_type; /* Type of event. */
	void *data;  /* Pointer to event data of event type. */
};

 enum event_type{
    CLI		= 1,
    LOG		= 2
 };


extern pthread_cond_t g_eventmanager_signal;
extern struct event_head g_eventmanager_queue;

void *eventmanager_function (void *dummyPtr);

#endif /*EVENTMANAGER_H_*/
