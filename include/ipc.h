/**
 * @file ipc.h
 * @author Justin Yaple <yaplej@opennop.org>
 * @date 3 Jun 2014
 * @brief Header file for IPC communications between OpenNOP nodes.
 *
 * The IPC header provides functionality to verify neighboring nodes
 * in a OpenNOP @Domain or @Stub-Domain.
 * @see http://www.stack.nl/~dimitri/doxygen/docblocks.html
 * @see http://www.stack.nl/~dimitri/doxygen/commands.html
 */

#ifndef	_IPC_H_
#define	_IPC_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#define IPC_MAX_MESSAGE_SIZE	1024				/** Largest size the IPC messages can be */
#define OPENNOPD_IPC_PORT 		5000				/** Random number for now */
#define	OPENNOPD_IPC_SOCK		"\0opennopd.ipc"	/** '\0' makes this sock hidden */
#define OPENNOP_IPC_HELLO		10000				/** Timeout for epoll */

typedef enum {
    DOWN,			// Remote system not functioning or authorized.
    ATTEMPT,		// Establishing communication channel.
    ESTABLISHED,	// Connection established and verified.
    UP				// Neighbor is fully operational.
} neighborstate;

struct neighbor_head {
    struct neighbor *next; // Points to the first neighbor of the list.
    struct neighbor *prev; // Points to the last neighbor of the list.
    pthread_mutex_t lock; // Lock for this neighbor.
};
/**
 * UUID is only 16 bytes.
 * @see http://linux.die.net/man/3/uuid_generate
 */
#define OPENNOP_IPC_UUID_LENGTH		16
#define OPENNOP_IPC_KEY_LENGTH		65
#define OPENNOP_IPC_NAME_LENGTH		64

struct neighbor {
    struct neighbor *next;
    struct neighbor *prev;
    char name[OPENNOP_IPC_NAME_LENGTH]; // Unique name for this neighbor.
    __u32 NeighborIP; // IP address of this neighbor.
    neighborstate state; // Detected state of this neighbor.
    char UUID[OPENNOP_IPC_UUID_LENGTH]; // Detected ID of this neighbor.
    int sock; // Socket FD used to communicate to this neighbor.
    char key[OPENNOP_IPC_KEY_LENGTH]; // Encryption key used by this neighbor.
    time_t timer; // Remote timer.
};

#define OPENNOP_DEFAULT_HEADER_LENGTH	8

struct opennop_ipc_header {
    __u8 type;				/** OpenNOP IPC type */
    __u8 header_length;		/** OpenNOP IPC header length */
    __u16 version;			/** Version of the message system */
    __u16 length;			/** Total length of this message */
    __u8 security;			/** Indicates if security is required */
    __u8 antireplay;		/** Indicates if anti-replay is enabled */
};

struct opennop_header_data {
    char *securitydata;		/** SHA256 data of the entire message */
    __u32 *antireplaydata;	/** Anti-replay data */
    char *messages;			/** Beginning of any OpenNOP messages */
};

struct opennop_message_header {
    __u16 type;				/** OpenNOP message type */
    __u16 length;			/** Total length of this message */
};

/**
 * @see http://linux.die.net/man/3/uuid_generate
 */
struct opennop_hello_message{
	struct opennop_message_header header;
	char uuid[OPENNOP_IPC_UUID_LENGTH];
};

struct ipc_message_i_see_you{
	struct opennop_message_header header;
	/*
	 * Additional data would go here.
	 */
};

typedef enum {
    OPENNOP_MSG_TYPE_IPC = 1,
    OPENNOP_MSG_TYPE_CLI,
    OPENNOP_MSG_TYPE_DRV
} OPENNOP_MSG_TYPE;

#define OPENNOP_MSG_VERSION 	1

typedef enum {
    OPENNOP_MSG_SECURITY_NO = 0,
    OPENNOP_MSG_SECURITY_SHA
} OPENNOP_MSG_SECURITY;

enum {
    OPENNOP_MSG_ANTI_REPLAY_NO = 0,
    OPENNOP_MSG_ANTI_REPLAY_YES
};

typedef enum {
    OPENNOP_IPC_HERE_I_AM = 1,
    OPENNOP_IPC_I_SEE_YOU,
    OPENNOP_IPC_AUTH_ERR,
    OPENNOP_IPC_BAD_UUID,
    OPENNOP_IPC_DEDUP_MAP
} OPENNOP_IPC_MSG_TYPE;

void start_ipc();
void rejoin_ipc();
int verify_neighbor_in_domain(__u32 neighborIP);

#endif
