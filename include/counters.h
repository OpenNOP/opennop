#ifndef COUNTERS_H_
#define COUNTERS_H_

#include <pthread.h>
#include <sys/time.h>
#include <linux/types.h>

typedef void *t_counterdata;
typedef void (*t_counterfunction)(t_counterdata);

struct counter_head {
	struct counter *next; // Points to the first command of the list.
	struct counter *prev; // Points to the last command of the list.
	pthread_mutex_t lock; // Lock for this node.
};

struct counter {
	struct counter *next;
	struct counter *prev;
	t_counterfunction handler;
	t_counterdata data; //Points to the metric structure.
};

void *counters_function(void *dummyPtr);
int calculate_ppsbps(__u32 previouscount, __u32 currentcount);
int register_counter(t_counterfunction, t_counterdata); //Adds a counter record to the counters head list.
int un_register_counter(t_counterfunction, t_counterdata); //Will remove a counter record.
struct counter* allocate_counter();
void execute_counters();

#endif /*COUNTERS_H_*/
