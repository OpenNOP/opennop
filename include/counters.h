#ifndef COUNTERS_H_
#define COUNTERS_H_

#include <pthread.h>
#include <sys/time.h>
#include <linux/types.h>

struct counters {
	/*
	 * number of packets processed.
	 * used for pps calculation. can roll.
	 * should increment at end of main loop after each packet is finished processing.
	 */
	__u32 packets;
	__u32 pps; // Where the calculated pps are stored.

	/*
	 * number of bytes entering process.
	 * used for bps calculation. can roll.
	 * should increment as beginning of main loop after packet is received from queue.
	 */
	__u32 bytesin;
	__u32 bpsin; // Where the calculated bps are stored.
	__u32 bytesout;
	__u32 bpsout; // Where the calculated bps are stored.

	/*
	 * traffic optimized/un-optimized counters.
	 * should increment only if packet is optimized/un-optimized.
	 * not used by fetcher.
	 */
	__u16 B, KB, MB, GB;

	/*
	 * Stores when the counters were last updated.
	 */
	struct timeval updated;

	/*
	 * When something reads/writes to this counter it should use the lock.
	 */
	pthread_mutex_t lock;
};

void *counters_function(void *dummyPtr);
int calculate_ppsbps(__u32 previouscount, __u32 currentcount);
int cli_show_counters(int client_fd, char **parameters, int numparameters);
struct counters get_counters(struct counters *thiscounter);
void set_pps(struct counters *thiscounter, __u32 count);
void set_bpsin(struct counters *thiscounter, __u32 count);
void set_bpsout(struct counters *thiscounter, __u32 count);

#endif /*COUNTERS_H_*/
