#ifndef	_IPC_H_
#define	_IPC_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/socket.h>

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
    char UUID[16];
    int sock;
    char key[64];
};

typedef int (*t_node_command)(int, __u32, char *);

void start_ipc();
int cli_node(int client_fd, char **parameters, int numparameters);
int cli_no_node(int client_fd, char **parameters, int numparameters);
int cli_show_nodes(int client_fd, char **parameters, int numparameters);
int validate_node_input(int client_fd, char *stringip, char *key, t_node_command node_command);
int add_update_node(int client_fd, __u32 nodeIP, char *key);
int del_node(int client_fd, __u32 nodeIP, char *key);
int cli_node_help(int client_fd);
struct node* allocate_node(__u32 nodeIP, char *key);

#endif
