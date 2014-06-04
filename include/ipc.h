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

#define IPC_MAX_MESSAGE_SIZE	1024
#define OPENNOPD_IPC_PORT 5000 // Random number for now.
#define	OPENNOPD_IPC_SOCK	"\0opennopd.ipc" // '\0' makes this sock hidden.

typedef enum {
	DOWN, // Remote system not functioning or authorized.
	ATTEMPT, // Establishing communication channel.
	ESTABLISHED // Connection established and verified.
} neighborstate;

struct neighbor_head
{
    struct neighbor *next; // Points to the first neighbor of the list.
    struct neighbor *prev; // Points to the last neighbor of the list.
    pthread_mutex_t lock; // Lock for this neighbor.
};

#define OPENNOP_IPC_UUID_LENGTH		33
#define OPENNOP_IPC_KEY_LENGTH		65

struct neighbor {
    struct neighbor *next;
    struct neighbor *prev;
    char name[65]; // Unique name for this neighbor.
    __u32 NeighborIP; // IP address of this neighbor.
    neighborstate state; // Detected state of this neighbor.
    char UUID[OPENNOP_IPC_UUID_LENGTH]; // Detected ID of this neighbor.
    int sock; // Socket FD used to communicate to this neighbor.
    char key[OPENNOP_IPC_KEY_LENGTH]; // Encryption key used by this neighbor.
    time_t timer; // Remote timer.
};

#define OPENNOP_DEFAULT_HEADER_LENGTH	8

struct opennop_message_header {
    __u16 type;				/** OpenNOP message type */
    __u16 version;			/** Version of the message system */
    __u16 length;			/** Total length of this message */
    __u8 security;			/** Indicates if security is required */
    __u8 antireplay;		/** Indicates if anti-replay is enabled */
};

struct opennop_message_data {
    char *securitydata;		/** SHA256 data of the entire message */
    __u32 *antireplaydata;	/** Anti-replay data */
    char *messages;			/** Beginning of any OpenNOP messages */
};

enum {
	OPENNOP_MSG_TYPE_IPC = 1,
	OPENNOP_MSG_TYPE_CLI,
	OPENNOP_MSG_TYPE_DRV
};

#define OPENNOP_MSG_VERSION 	1

enum {
	OPENNOP_MSG_SECURITY_NO = 0,
	OPENNOP_MSG_SECURITY_SHA
};

enum {
	OPENNOP_MSG_ANTI_REPLAY_NO = 0,
	OPENNOP_MSG_ANTI_REPLAY_YES
};

enum {
	OPENNOP_IPC_HERE_I_AM = 1,
	OPENNOP_IPC_I_SEE_YOU,
	OPENNOP_IPC_AUTH_ERR,
	OPENNOP_IPC_BAD_UUID,
	OPENNOP_IPC_DEDUP_MAP
};

void start_ipc();
void rejoin_ipc();
int verify_neighbor_in_domain(__u32 neighborIP);
/*
 * Adding this for some debugging.  There really is not any need for it to be a public function.
 */
int print_opennnop_header(struct opennop_message_header *opennop_msg_header);
int hello_neighbors(void);

#endif
