#ifndef	_IPC_H_
#define	_IPC_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_BUFFER_SIZE	1024
#define OPENNOPD_IPC_PORT 5000 // Random number for now.
#define	OPENNOPD_IPC_SOCK	"\0opennopd.ipc"

struct node_head
{
    struct node *next; // Points to the first node of the list.
    struct node *prev; // Points to the last node of the list.
    pthread_mutex_t lock; // Lock for this node.
};

struct node {
    struct node *next;
    struct node *prev;
    __u32 NodeIP;
    char UUID[17];
    int sock;
    char key[65];
};

typedef int (*t_node_command)(int, __u32, char *);

void start_ipc();
void rejoin_ipc();
int verify_node_in_domain(__u32 nodeIP);

#endif
