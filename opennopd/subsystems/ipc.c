#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h> // for multi-threading
#include <sys/socket.h>
#include <sys/un.h>

#include <linux/types.h>
#include <arpa/inet.h>

#include "ipc.h"
#include "clicommands.h"
#include "logger.h"

/*
 * I was using "head" and "tail" here but that seemed to conflict with another module.
 * Should the internal variable names be isolated between modules?
 * They don't appear to be clicommands.c also uses a variable "head".
 */
struct node_head ipchead;

void start_ipc() {
    /*
     * This will setup and start the IPC threads.
     * We also register the IPC commands here.
     */
    register_command("node", cli_node, true, false);
    register_command("no node", cli_no_node, true, false);
    register_command("show nodes", cli_show_nodes, false, false);

    ipchead.next = NULL;
    ipchead.prev = NULL;

}

void *ipc_listener_thread(void *dummyPtr) {

    /*
     * This thread listens for new connections from nodes.
     * If the thread is new it stores it in the node "sock".
     */

    return NULL;
}

void *ipc_thread(void *dummyPtr) {
    /*
     * This thread will have to monitor for any outbound messages
     * and send them to the appropriate queue.  It should test
     * that the sock is active for the node.
     *
     * If we can use this same thread to poll all open sockets
     * and respond to those messages here we should do that also.
     */

    return NULL;
}

/*
 * We only need to validate the number of parameters are correct here.
 * Lets let the more specific functions do the data validation from the input.
 * This should accept 1 or 2 parameters.
 * parameter[0] = IP in string format.
 * parameter[1] = Authentication key(optional).
 */
int cli_node(int client_fd, char **parameters, int numparameters) {
    int ERROR = 0;

    if ((numparameters < 1) || (numparameters > 2)) {
        ERROR = cli_node_help(client_fd);

    } else if (numparameters == 1) {
        ERROR = validate_node_input(client_fd, parameters[0], NULL, &add_update_node);

    } else if (numparameters == 2) {
        ERROR = validate_node_input(client_fd, parameters[0], parameters[1], &add_update_node);
    }

    return 0;
}

/*
 * We only need to validate the number of parameters are correct here.
 * Lets let the more specific functions do the data validation from the input.
 * This should accept 1 parameter.
 * parameter[0] = IP in string format.
 */
int cli_no_node(int client_fd, char **parameters, int numparameters) {
    int ERROR = 0;

    if (numparameters == 1) {
        ERROR = validate_node_input(client_fd, parameters[0], NULL, &del_node);

    } else {
        ERROR = cli_node_help(client_fd);
    }

    return 0;
}

int cli_show_nodes(int client_fd, char **parameters, int numparameters) {
    struct node *currentnode = NULL;
    char temp[20];
    char col1[17];
    char col2[18];
    char col3[66];
    char end[3];
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Show nodes in OpenNOP.\n");
    cli_send_feedback(client_fd, msg);

    currentnode = ipchead.next;

    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);
    sprintf(
        msg,
        "|    Node IP      |       GUID       |                               Key                                |\n");
    cli_send_feedback(client_fd, msg);
    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);

    while(currentnode != NULL) {
        strcpy(msg, "");
        inet_ntop(AF_INET, &currentnode->NodeIP, temp,
                  INET_ADDRSTRLEN);
        sprintf(col1, "| %-16s", temp);
        strcat(msg, col1);
        sprintf(col2, "| %-17s", currentnode->UUID);
        strcat(msg, col2);
        sprintf(col3, "| %-65s", currentnode->key);
        strcat(msg, col3);
        sprintf(end, "|\n");
        strcat(msg, end);
        cli_send_feedback(client_fd, msg);

        currentnode = currentnode->next;
    }

    return 0;
}

int validate_node_input(int client_fd, char *stringip, char *key, t_node_command node_command) {
    int ERROR = 0;
    int keylength = 0;
    __u32 nodeIP = 0;
    char msg[MAX_BUFFER_SIZE] = { 0 };

    /*
     * We must validate the user data here
     * before adding or updating anything.
     * 1. stringip should convert to an integer using inet_pton()
     * 2. key should be NULL or < 64 bytes
     */

    ERROR = inet_pton(AF_INET, stringip, &nodeIP); //Should return 1.

    if(ERROR != 1) {
        return cli_node_help(client_fd);
    }

    if(key != NULL) {
        keylength = strlen(key);

        if(keylength > 64) {
            return cli_node_help(client_fd);
        }
    }

    /*
     * The input is valid so we run the specified command
     * This will add, update or remove a node.
     */
    node_command(client_fd, nodeIP, key);

    sprintf(msg,"Node string IP is [%s].\n", stringip);
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Node integer IP is [%u].\n", ntohl(nodeIP));
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Node key is [%s].\n", key);
    cli_send_feedback(client_fd, msg);

    if(key != NULL) {
        sprintf(msg,"Node key length is [%u].\n", keylength);
        cli_send_feedback(client_fd, msg);
    }

    return 0;
}

int add_update_node(int client_fd, __u32 nodeIP, char *key) {
    struct node *currentnode = NULL;
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Add or update a node.\n");
    cli_send_feedback(client_fd, msg);

    currentnode = ipchead.next;

    /*
     * Make sure the node does not already exist.
     */
    while (currentnode != NULL) {
        sprintf(msg,"Searching for node.\n");
        cli_send_feedback(client_fd, msg);

        if (currentnode->NodeIP == nodeIP) {
            sprintf(msg,"Node already exists.\n");
            cli_send_feedback(client_fd, msg);

            if(key != NULL) {
                strcpy(currentnode->key,key);
            }

            return 0;
        }

        currentnode = currentnode->next;
    }

    /*
     * Did not find the node so lets add it.
     */
    sprintf(msg,"Creating a new node.\n");
    cli_send_feedback(client_fd, msg);

    currentnode = allocate_node(nodeIP, key);

    sprintf(msg,"Allocated a new node.\n");
    cli_send_feedback(client_fd, msg);

    if (currentnode != NULL) {

        if (ipchead.next == NULL) {
            ipchead.next = currentnode;
            ipchead.prev = currentnode;

        } else {
            currentnode->prev = ipchead.prev;
            ipchead.prev->next = currentnode;
            ipchead.prev = currentnode;
        }

    }

    return 0;
}

int del_node(int client_fd, __u32 nodeIP, char *key) {
    struct node *currentnode = NULL;
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Remove node from OpenNOP.\n");
    cli_send_feedback(client_fd, msg);

    currentnode = ipchead.next;

    while (currentnode != NULL) {
        sprintf(msg,"Searching for node.\n");
        cli_send_feedback(client_fd, msg);

        if (currentnode->NodeIP == nodeIP) {
            sprintf(msg,"Node exists.\n");
            cli_send_feedback(client_fd, msg);

            if((currentnode->prev == NULL) && (currentnode->next == NULL)) {
                ipchead.next = NULL;
                ipchead.prev = NULL;

            } else if ((currentnode->prev == NULL) && (currentnode->next != NULL)) {
                ipchead.next = currentnode->next;
                currentnode->next->prev = NULL;

            } else if ((currentnode->prev != NULL) && (currentnode->next == NULL)) {
                ipchead.prev = currentnode->prev;
                currentnode->prev->next = NULL;

            } else if ((currentnode->next != NULL) && (currentnode->prev != NULL)) {
                currentnode->prev->next = currentnode->next;
                currentnode->next->prev = currentnode->prev;
            }

            free(currentnode);
            currentnode = NULL;

            return 0;
        }

        currentnode = currentnode->next;
    }

    return 0;
}

int cli_node_help(int client_fd) {
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Usage: [no] node <ip address> [key]\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

struct node* allocate_node(__u32 nodeIP, char *key) {
    struct node *newnode = (struct node *) malloc (sizeof (struct node));

    if(newnode == NULL) {
        fprintf(stdout, "Could not allocate memory... \n");
        exit(1);
    }
    newnode->next = NULL;
    newnode->prev = NULL;
    newnode->NodeIP = 0;
    newnode->UUID[0] = '\0';
    newnode->sock = 0;
    newnode->key[0] = '\0';

    if (nodeIP != 0) {
        newnode->NodeIP = nodeIP;
    }

    if (key != NULL) {
        strcpy(newnode->key,key);
    }

    return newnode;
}
