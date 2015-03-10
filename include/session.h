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

struct endpoint {
	__u32 address;
	__u16 port;
	__u32 sequence;
	__u32 nextsequence;
	char accelerator[OPENNOP_IPC_ID_LENGTH];
};

/* Structure used to store TCP session info. */
struct session {
	struct session_head *head; // Points to the head of this list.
	struct session *next; // Points to the next session in the list.
	struct session *prev; // Points to the previous session in the list.
	__u32 *client; // Points to the client IP Address.
	__u32 *server; // Points to the server IP Address.
	struct endpoint larger;
	__u32 largerIPStartSEQ; // Stores the starting SEQ number.
	__u32 largerIPNextAck;
	struct endpoint smaller;
	__u32 smallerIPStartSEQ; // Stores the starting SEQ number.
	__u32 smallerIPNextAck;
	__u8 deadcounter; // Stores how many counts the session has been idle.
	__u8 state; // Stores the TCP session state.
	__u8 queue; // What worker queue the packets for this session go to.
};


#endif /*SESSION_H_*/
