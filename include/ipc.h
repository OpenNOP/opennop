#ifndef	_IPC_H_
#define	_IPC_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#define IPC_MESSAGE_SIZE	1024
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

struct neighbor {
    struct neighbor *next;
    struct neighbor *prev;
    char name[65]; //unique name for the remote OpenNOP accelerator.
    __u32 NeighborIP; // IP address of a remote OpenNOP accelerator.
    neighborstate state; // Detected state of this remote.
    char UUID[17]; // Detected ID of this remote.
    int sock; // Socket FD used to communicate to this remote.
    char key[65]; // Encryption key used by this remote.
    time_t timer; // Remote timer.
};

typedef int (*t_neighbor_command)(int, __u32, char *);

void start_ipc();
void rejoin_ipc();
int verify_neighbor_in_domain(__u32 neighborIP);

#endif
