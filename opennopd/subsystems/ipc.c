#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h> // for multi-threading
#include <sys/socket.h>
#include <sys/un.h>

#include <linux/types.h>

#include "ipc.h"
#include "clicommands.h"
#include "logger.h"

struct node *head;
struct node *tail;

void start_ipc() {
    /*
     * This will setup and start the IPC threads.
     * We also register the IPC commands here.
     */
    register_command("node", cli_node, true, false);
    register_command("no node", cli_no_node, true, false);
    register_command("show nodes", cli_show_nodes, false, false);
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
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Add node to OpenNOP.\n");
    cli_send_feedback(client_fd, msg);

    if ((numparameters < 1) || (numparameters > 2)) {
        ERROR = cli_node_help(client_fd);
    } else if (numparameters == 1) {
        ERROR = validate_node_input(client_fd, parameters[0], NULL);
    } else if (numparameters == 2) {
        ERROR = validate_node_input(client_fd, parameters[0], parameters[1]);
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
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Remove node from OpenNOP.\n");
    cli_send_feedback(client_fd, msg);

    if (numparameters == 1) {
        ERROR = del_node(client_fd, parameters[0]);
    } else {
        ERROR = cli_node_help(client_fd);
    }

    return 0;
}

int cli_show_nodes(int client_fd, char **parameters, int numparameters) {
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Show nodes in OpenNOP.\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

int validate_node_input(int client_fd, char *stringip, char *key) {
    int ERROR = 0;
    char validkey[64] = { 0 };
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

        if(keylength <= 64) {
            strcpy(validkey,key);
        }
    }

    sprintf(msg,"Node string IP is [%s].\n", stringip);
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Node integer IP is [%u].\n", ntohl(nodeIP));
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Node key is [%s].\n", key);
    cli_send_feedback(client_fd, msg);

    if(key != NULL) {
        sprintf(msg,"Node key length is [%u].\n", strlen(key));
        cli_send_feedback(client_fd, msg);
    }

    sprintf(msg,"Node valid key is [%s].\n", validkey);
    cli_send_feedback(client_fd, msg);

    return 0;
}

int del_node(int client_fd, char *stringip) {

    return 0;
}

int cli_node_help(int client_fd) {
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Usage: [no] node <ip address> [key]\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

