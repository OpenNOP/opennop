#ifndef SESSION_H_
#define SESSION_H_

#include <sys/types.h>
#include <linux/types.h>
#include "ipc.h"

/* Structure used for the head of a session list. */
struct session_head {
	struct session *next; /* Points to the first session of the list. */
	struct session *prev; /* Points to the last session of the list. */
	u_int32_t qlen; // Total number of sessions in the list.
	pthread_mutex_t lock; // Lock for this session bucket.
};

/* Structure used to store TCP session info. */
struct session {
	struct session_head *head; // Points to the head of this list.
	struct session *next; // Points to the next session in the list.
	struct session *prev; // Points to the previous session in the list.
	__u32 *client; // Points to the client IP Address.
	__u32 *server; // Points to the server IP Address.
	__u32 largerIP; // Stores the larger IP address.
	__u16 largerIPPort; // Stores the larger IP port #.
	__u32 largerIPStartSEQ; // Stores the starting SEQ number.
	__u32 largerIPseq; // Stores the TCP SEQ from the largerIP.
	__u32 largerIPPreviousseq; // Stores the TCP SEQ from the largerIP.
	__u32 largerIPNextAck;
	char largerIPAccelerator[OPENNOP_IPC_ID_LENGTH]; // Stores the AcceleratorIP of the largerIP.
	__u32 smallerIP; // Stores the smaller IP address.
	__u16 smallerIPPort; // Stores the smaller IP port #.
	__u32 smallerIPStartSEQ; // Stores the starting SEQ number.
	__u32 smallerIPseq; // Stores the TCP SEQ from the smallerIP.
	__u32 smallerIPPreviousseq; // Stores the TCP SEQ from the smallerIP.
	__u32 smallerIPNextAck;
	char smallerIPAccelerator[OPENNOP_IPC_ID_LENGTH]; // Stores the AcceleratorIP of the smallerIP.
	__u8 deadcounter; // Stores how many counts the session has been idle.
	__u8 state; // Stores the TCP session state.
	__u8 queue; // What worker queue the packets for this session go to.
};


#endif /*SESSION_H_*/
