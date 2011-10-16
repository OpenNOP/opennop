#ifndef PACKET_H_
#define PACKET_H_

#define BUFSIZE 2048 // Size of buffer used to store IP packets.

/* Structure used for the head of a packet queue.. */
struct packet_head{
	struct packet *next; // Points to the first packet of the list.
	struct packet *prev; // Points to the last packet of the list.
	u_int32_t qlen; // Total number of packets in the list.
	pthread_mutex_t lock; // Lock for this queue.
};

struct packet {
	struct packet_head *head; // Points to the head of this list.
	struct packet *next; // Points to the next packet.
	struct packet *prev; // Points to the previous packet.
	struct nfq_q_handle *hq; // The Queue Handle to the Netfilter Queue.
	u_int32_t id; // The ID of this packet in the Netfilter Queue.
	unsigned char data[BUFSIZE]; // Stores the actual IP packet.
};

#endif /*PACKET_H_*/
