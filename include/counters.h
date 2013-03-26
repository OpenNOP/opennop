#ifndef COUNTERS_H_
#define COUNTERS_H_

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
	__u32 bytes;
	__u32 bps; // Where the calculated bps are stored.

	/*
	 * traffic optimized/un-optimized counters.
	 * should increment only if packet is optimized/un-optimized.
	 * not used by fetcher.
	 */
	__u16 B, KB, MB, GB;
};

void *counters_function (void *dummyPtr);
int calculate_ppsbps(__u32 previouscount, __u32 currentcount);

#endif /*COUNTERS_H_*/
