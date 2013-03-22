#ifndef COUNTERS_H_
#define COUNTERS_H_

struct counters {
	/*
	 * number of packets processed.
	 * used for pps calculation. can roll.
	 * should increment at end of main loop after each packet is finished processing.
	 */
	__u32 packets;

	/*
	 * number of bytes entering process.
	 * used for bps calculation. can roll.
	 * should increment as beginning of main loop after packet is received from queue.
	 */
	__u32 bytes;

	/*
	 * traffic optimized/un-optimized counters.
	 * should increment only if packet is optimized/un-optimized.
	 * not used by fetcher.
	 */
	__u16 B, KB, MB, GB;
};

#endif /*COUNTERS_H_*/
