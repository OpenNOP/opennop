#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h> // for multi-threading
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>

#include <linux/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ipc.h"
#include "clicommands.h"
#include "logger.h"

#define MAXEVENTS 64

/*
 * I was using "head" and "tail" here but that seemed to conflict with another module.
 * Should the internal variable names be isolated between modules?
 * They don't appear to be clicommands.c also uses a variable "head".
 * Using "static" should fix this.
 */
static pthread_t t_ipc; // thread for cli.
static struct node_head ipchead;
static char UUID[17]; //Local UUID.
static char key[65]; //Local key.

/*
 * Create an IP socket for the IPC.
 */
int new_ip_server(int port) {
    int server_socket = 0;
    int error = 0;
    struct sockaddr_in server = {
                                    0
                                };
    char message[LOGSZ] = { 0 };

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0 ) {
        sprintf(message, "IPC: Failed to create socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    error = bind(server_socket, (struct sockaddr *)&server, sizeof(server));

    if (error < 0) {
        sprintf(message, "IPC: Failed to bind socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    error = listen(server_socket, SOMAXCONN);

    if (error == -1) {
        sprintf(message, "IPC: Could not listen on socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    return server_socket;
}

int accept_ip_client(int server_socket) {
    int newclient_socket = 0;
    int error = 0;
    struct sockaddr_in client = {
                                    0
                                };
    socklen_t in_len = 0;
    char hbuf[NI_MAXHOST] = {0};
    char sbuf[NI_MAXSERV] = {0};
    char message[LOGSZ] = { 0 };

    in_len = sizeof client;

    newclient_socket = accept(server_socket, (struct sockaddr *)&client, &in_len );

    if (newclient_socket == -1) {

        if ((errno == EAGAIN) ||
                (errno == EWOULDBLOCK)) {
            /*
             * We have processed all incoming connections.
             */
            return -1;

        } else {
            sprintf(message, "IPC: Failed to accept socket.\n");
            logger(LOG_INFO, message);
            return -1;
        }
    }

    error = getnameinfo((struct sockaddr*)&client, sizeof client,
                        hbuf, sizeof hbuf,
                        sbuf, sizeof sbuf,
                        NI_NUMERICHOST | NI_NUMERICSERV);

    if (error == 0) {
        sprintf(message,"IPC: Accepted connection on descriptor %d "
                "(host=%s, port=%s)\n", newclient_socket, hbuf, sbuf);
    }

    return newclient_socket;
}

int new_unix_client(char* path) {
    int client_socket = 0;
    int error = 0;
    struct sockaddr_un client = {
                                    0
                                };
    char message[LOGSZ];

    client_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (client_socket < 0 ) {
        sprintf(message, "IPC: Failed to create socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    client.sun_family = AF_UNIX;
    strncpy(client.sun_path, path,sizeof(client.sun_path)-1);
    unlink(client.sun_path);

    error = connect(client_socket, (struct sockaddr *)&client, sizeof(client));

    if (error < 0) {
        sprintf(message, "IPC: Failed to connect.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    return client_socket;
}


/*
 * Creates a new UNIX domain server socket.
 * http://troydhanson.github.io/misc/Unix_domain_sockets.html
 */
int new_unix_server(char* path) {
    int server_socket = 0;
    int error = 0;
    struct sockaddr_un server = {
                                    0
                                };
    char message[LOGSZ];

    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_socket < 0 ) {
        sprintf(message, "IPC: Failed to create socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, path,sizeof(server.sun_path)-1);
    unlink(server.sun_path);

    error = bind(server_socket, (struct sockaddr *)&server, sizeof(server));

    if (error < 0) {
        sprintf(message, "IPC: Failed to bind socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    error = listen(server_socket, SOMAXCONN);

    if (error == -1) {
        sprintf(message, "IPC: Could not listen on socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    return server_socket;
}

int accept_unix_client(int server_socket) {
    int newclient_socket = 0;
    int error = 0;
    struct sockaddr_un client = {
                                    0
                                };
    socklen_t in_len = 0;
    char message[LOGSZ] = { 0 };

    in_len = sizeof client;

    newclient_socket = accept(server_socket, (struct sockaddr *)&client, &in_len );

    if (newclient_socket == -1) {

        if ((errno == EAGAIN) ||
                (errno == EWOULDBLOCK)) {
            /*
             * We have processed all incoming connections.
             */
            return -1;

        } else {
            sprintf(message, "IPC: Failed to accept socket.\n");
            logger(LOG_INFO, message);
            return -1;
        }
    }

    return newclient_socket;
}

int make_socket_non_blocking (int socket) {
    int flags, s;
    char message[LOGSZ];

    flags = fcntl (socket, F_GETFL, 0);
    if (flags == -1) {
        sprintf(message, "IPC: Failed getting socket flags.\n");
        logger(LOG_INFO, message);
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(socket, F_SETFL, flags);
    if (s == -1) {
        sprintf(message, "IPC: Failed setting socket flags.\n");
        logger(LOG_INFO, message);
        return -1;
    }

    return 0;
}

int register_socket(int listener_socket, int epoll_fd, struct epoll_event *event) {
    int error = 0;
    char message[LOGSZ] = {0};

    if (listener_socket < 0) {
        sprintf(message, "IPC: Failed to get socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    error = make_socket_non_blocking(listener_socket);

    if (error == -1) {
        sprintf(message, "IPC: Failed setting socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    event->data.fd = listener_socket;
    event->events = EPOLLIN | EPOLLET;
    error = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener_socket, event);

    if (error == -1) {
        sprintf(message, "IPC: Failed adding remote listener to epoll instance.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    return error;
}

/*
 * This thread will monitor all IPC events.
 * It has to monitor in-bound connection for remote IPC
 * and also handle in-bound local IPC from the CLI.
 *
 * The CLI will use the local IPC to notify the IPC thread
 * that new fd's need to be setup and monitored.
 * For events like adding/removing neighbors from the IPC.
 *
 * Thanks to Mukund Sivaraman for the tutorial on epoll().
 * https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
 * http://man7.org/linux/man-pages/man7/epoll.7.html
 * http://www.cs.rutgers.edu/~pxk/rutgers/notes/sockets/
 * http://www.beej.us/guide/bgnet/output/html/multipage/fcntlman.html
 */
void *ipc_thread(void *dummyPtr) {
    int remote_listener_socket = 0; // Listens for connections from remote neighbors.
    int local_listener_socket = 0; // Listens for connections from the local CLI.
    int client_socket = 0;
    int epoll_fd = 0;
    int error = 0;
    int numevents = 0;
    int done = 0;
    int i = 0;
    ssize_t count;
    char buf[MAX_BUFFER_SIZE];
    struct epoll_event event = {
                                   0
                               };
    struct epoll_event *events = NULL;
    char message[LOGSZ] = {0};

    sprintf(message, "IPC: Is starting.\n");
    logger(LOG_INFO, message);

    events = calloc (MAXEVENTS, sizeof event);

    epoll_fd = epoll_create1(0);

    if(epoll_fd == -1) {
        sprintf(message, "IPC: Could not create epoll instance.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    /*
     * First we setup the remote IPC listener socket.
     * This accepts connections from the remote neighbors.
     */

    remote_listener_socket = new_ip_server(OPENNOPD_IPC_PORT);

    error = register_socket(remote_listener_socket, epoll_fd, &event);

    /*
     * Second we setup the local IPC listener socket.
     * This accepts connections from the local CLI.
     */
    local_listener_socket = new_unix_server(OPENNOPD_IPC_SOCK);

    error = register_socket(local_listener_socket, epoll_fd, &event);

    /*
     * Third we listen for events and handle them.
     */
    while(1) {
        numevents = epoll_wait (epoll_fd, events, MAXEVENTS, -1);

        for (i = 0; i < numevents; i++) {

            if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                /*
                 * An error has occurred on this fd, or the socket is not
                 * ready for reading (why were we notified then?)
                 */

                sprintf(message, "IPC: Epoll error.\n");
                logger(LOG_INFO, message);

                close (events[i].data.fd);

                continue;

            } else if (events[i].data.fd == remote_listener_socket) {
                /*
                 * We have a notification on the listening socket,
                 * which means one or more incoming connections.
                 */
                while (1) {
                    client_socket = accept_ip_client(remote_listener_socket);

                    if (client_socket == -1) {
                        break;
                    }

                    error = make_socket_non_blocking(client_socket);

                    if (error == -1) {
                        sprintf(message, "IPC: Failed setting socket on client.\n");
                        logger(LOG_INFO, message);
                        exit(1);
                    }

                    error = register_socket(client_socket, epoll_fd, &event);

                    continue;
                }

            } else if (events[i].data.fd == local_listener_socket) {
                /*
                 * We have a notification on the listening socket,
                 * which means one or more incoming connections.
                 */
                while (1) {
                    client_socket = accept_unix_client(local_listener_socket);

                    if (client_socket == -1) {
                        break;
                    }

                    error = make_socket_non_blocking(client_socket);

                    if (error == -1) {
                        sprintf(message, "IPC: Failed setting socket on client.\n");
                        logger(LOG_INFO, message);
                        exit(1);
                    }

                    error = register_socket(client_socket, epoll_fd, &event);

                    continue;
                }

            } else {
                /*
                 * We have data on the fd waiting to be read. Read and
                 * display it. We must read whatever data is available
                    * completely, as we are running in edge-triggered mode
                    * and won't get a notification again for the same
                    * data.
                 */
                while(1) {
                    count = recv(events[i].data.fd, buf, MAX_BUFFER_SIZE, 0);

                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN) {
                            sprintf(message, "IPC: Failed reading message.\n");
                            logger(LOG_INFO, message);
                            done = 1;
                        }
                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = 1;
                        break;
                    }

                    /* Write the buffer to standard output */
                    //error = write (1, buf, count);
                    sprintf(message, "IPC: %s.\n",buf);
                    logger(LOG_INFO, message);

                    if (error == -1) {
                        sprintf(message, "IPC: Failed writing message.\n");
                        logger(LOG_INFO, message);
                        abort ();
                    }

                    if (done) {
                        sprintf(message, "IPC: Closed connection on descriptor %d\n",
                                events[i].data.fd);
                        logger(LOG_INFO, message);

                        /*
                         * Closing the descriptor will make epoll remove it
                         * from the set of descriptors which are monitored.
                         */
                        close (events[i].data.fd);
                    }
                }
            }
        }
    }

    sprintf(message, "IPC: Is exiting.\n");
    logger(LOG_INFO, message);


    free(events);
    close(remote_listener_socket);
    close(local_listener_socket);
    return EXIT_SUCCESS;
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
        strncpy(newnode->key, key, sizeof(newnode->key)-1);
    }

    return newnode;
}

int cli_node_help(int client_fd) {
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Usage: [no] node <ip address> [key]\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

struct commandresult cli_show_nodes(int client_fd, char **parameters, int numparameters, void *data) {
    struct node *currentnode = NULL;
    struct commandresult result  = {
                                       0
                                   };
    char temp[20];
    char col1[17];
    char col2[18];
    char col3[66];
    char end[3];
    char msg[MAX_BUFFER_SIZE] = { 0 };

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

    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

int add_update_node(int client_fd, __u32 nodeIP, char *key) {
    struct node *currentnode = NULL;

    currentnode = ipchead.next;

    /*
     * Make sure the node does not already exist.
     */
    while (currentnode != NULL) {

        if (currentnode->NodeIP == nodeIP) {

            if(key != NULL) {
                strncpy(currentnode->key, key, sizeof(currentnode->key)-1);
            }

            return 0;
        }

        currentnode = currentnode->next;
    }

    /*
     * Did not find the node so lets add it.
     */

    currentnode = allocate_node(nodeIP, key);

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

    currentnode = ipchead.next;

    while (currentnode != NULL) {

        if (currentnode->NodeIP == nodeIP) {

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

    /*
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
    */

    return 0;
}

/*
 * We only need to validate the number of parameters are correct here.
 * Lets let the more specific functions do the data validation from the input.
 * This should accept 1 parameter.
 * parameter[0] = IP in string format.
 */
struct commandresult cli_no_node(int client_fd, char **parameters, int numparameters, void *data) {
    int ERROR = 0;
    struct commandresult result = {
                                      0
                                  };

    if (numparameters == 1) {
        ERROR = validate_node_input(client_fd, parameters[0], NULL, &del_node);

    } else {
        ERROR = cli_node_help(client_fd);
    }

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

/*
 * We only need to validate the number of parameters are correct here.
 * Lets let the more specific functions do the data validation from the input.
 * This should accept 1 or 2 parameters.
 * parameter[0] = IP in string format.
 * parameter[1] = Authentication key(optional).
 */
struct commandresult cli_node(int client_fd, char **parameters, int numparameters, void *data) {
    int socket = 0;
    int ERROR = 0;
    struct commandresult result  = {
                                       0
                                   };
    char message[LOGSZ] = {0};

    result.mode = NULL;
    result.data = NULL;

    if ((numparameters < 1) || (numparameters > 2)) {
        ERROR = cli_node_help(client_fd);

    } else if (numparameters == 1) {
        ERROR = validate_node_input(client_fd, parameters[0], NULL, &add_update_node);

    } else if (numparameters == 2) {
        ERROR = validate_node_input(client_fd, parameters[0], parameters[1], &add_update_node);
    }

    socket = new_unix_client(OPENNOPD_IPC_SOCK);

    if (socket < 0) {
        sprintf(message, "IPC: CLI failed to connect.\n");
        result.finished = ERROR;

        return result;
    }

    ERROR = send(socket, "HELLO!", 6, 0);

    if (ERROR < 0) {
        sprintf(message, "IPC: CLI could not write to IPC socket.\n");
        result.finished = 1;

        return result;
    }

    result.finished = 0;

    return result;
}

/*
 * We need to verify that the remote accelerator is a member of this domain.
 * TODO: Later the UUID will need to be used instead of the IP address.
 */
int verify_node_in_domain(__u32 nodeIP) {
    struct node *currentnode = NULL;

    currentnode = ipchead.next;

    while (currentnode != NULL) {

        if (currentnode->NodeIP == nodeIP) {

            return 1;
        }

        currentnode = currentnode->next;
    }

    return 0;
}

void start_ipc() {
    /*
     * This will setup and start the IPC threads.
     * We also register the IPC commands here.
     */
    register_command(NULL, "node", cli_node, true, false);
    register_command(NULL, "no node", cli_no_node, true, false);
    register_command(NULL, "show nodes", cli_show_nodes, false, false);

    ipchead.next = NULL;
    ipchead.prev = NULL;

    pthread_create(&t_ipc, NULL, ipc_thread, (void *) NULL);

}

void rejoin_ipc() {
    pthread_join(t_ipc, NULL);
}
