#ifndef	_IPC_H_
#define	_IPC_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/socket.h>

struct node {
    struct node *next;
    struct node *prev;
    int NodeIP;
    char UUID[16];
    int sock;
    char key[64];
};

void start_ipc();
int cli_node(int client_fd, char **parameters, int numparameters);
int cli_no_node(int client_fd, char **parameters, int numparameters);
int cli_show_nodes(int client_fd, char **parameters, int numparameters);
int add_update_node(int client_fd, char *stringip, char *key);
int del_node(int client_fd, char *stringip);
int cli_node_help(int client_fd);

#endif
