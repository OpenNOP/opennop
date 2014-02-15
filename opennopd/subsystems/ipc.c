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
#include "sockets.h"

#define MAXEVENTS 64
#define EVENTTIMER 10000; //epoll timeout in ms for event handling

/*
 * I was using "head" and "tail" here but that seemed to conflict with another module.
 * Should the internal variable names be isolated between modules?
 * They don't appear to be clicommands.c also uses a variable "head".
 * Using "static" should fix this.
 */
static pthread_t t_ipc; // thread for cli.
static struct neighbor_head ipchead;
static char UUID[17]; //Local UUID.
static char key[65]; //Local key.

int ipc_neighbor_hello(int socket){
    char buf[IPC_MESSAGE_SIZE];
    int error;
    sprintf(buf,"Hello!\n");
    error = send(socket, buf, strlen(buf), 0);
    return error;
}

void hello_neighbors() {
    struct neighbor *currentneighbor = NULL;
    time_t currenttime;
    int error = 0;
    char message[LOGSZ] = {0};

    time(&currenttime);

    for(currentneighbor = ipchead.next; currentneighbor != NULL; currentneighbor = currentneighbor->next) {

        if((currentneighbor->state == DOWN) && (difftime(currenttime, currentneighbor->timer) >= 30)) {
        	currentneighbor->timer = currenttime;
            sprintf(message, "state is down & timer > 30\n");
            logger(LOG_INFO, message);

            /*
             * If neighbor socket = 0 open a new one.
             */
            if(currentneighbor->sock == 0) {
                error = new_ip_client(currentneighbor->NeighborIP,OPENNOPD_IPC_PORT);

                if(error > 0) {
                    currentneighbor->sock = error;
                    currentneighbor->state = ATTEMPT;

                }
            }

        }else if((currentneighbor->state >= ATTEMPT) && (difftime(currenttime, currentneighbor->timer) >= 10)){
            /*
             * todo:
             * If we were successful in opening a connection we should sent a hello message.
             * Write the hello message function.
             */
        	if(currentneighbor->sock != 0){
        		currentneighbor->timer = currenttime;
        		error = ipc_neighbor_hello(currentneighbor->sock);

        		/*
        		 * Maybe a lot of this should be moved to ipc_neighbor_hello().
        		 */
        		if (error < 0){
        			 sprintf(message, "Failed sending hello.\n");
        			 logger(LOG_INFO, message);
        			 currentneighbor->state = DOWN;
        			 shutdown(currentneighbor->sock, SHUT_RDWR);
        			 currentneighbor->sock = 0;
        		}
        	}
        }
    }
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
    char buf[IPC_MESSAGE_SIZE];
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
        numevents = epoll_wait(epoll_fd, events, MAXEVENTS, 10000);

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

            	done = 0;  //Need to reset this for each message.
                while(1) {
                    count = recv(events[i].data.fd, buf, IPC_MESSAGE_SIZE, 0);

                    if(count > 0) {
                        buf[count - 1] = '\0';
                    }

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
                        sprintf(message, "IPC: Remote closed the connection.\n");
                        logger(LOG_INFO, message);
                        done = 1;
                        break;
                    }

                    /* Write the buffer to standard output */
                    //error = send(1, buf, count, 0);
                    sprintf(message, "IPC: %s\n",buf);
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
                        close(events[i].data.fd);
                    }
                }
            }
        }

        /*
         * Here is where we can execute other tasks.
         */
        hello_neighbors();
    }

    sprintf(message, "IPC: Is exiting.\n");
    logger(LOG_INFO, message);


    free(events);
    close(remote_listener_socket);
    close(local_listener_socket);
    return EXIT_SUCCESS;
}

struct neighbor* allocate_neighbor(__u32 neighborIP, char *key) {
    struct neighbor *newneighbor = (struct neighbor *) malloc (sizeof (struct neighbor));

    if(newneighbor == NULL) {
        fprintf(stdout, "Could not allocate memory... \n");
        exit(1);
    }
    newneighbor->next = NULL;
    newneighbor->prev = NULL;
    newneighbor->NeighborIP = 0;
    newneighbor->state = DOWN;
    newneighbor->UUID[0] = '\0';
    newneighbor->sock = 0;
    newneighbor->key[0] = '\0';
    time(&newneighbor->timer);

    if (neighborIP != 0) {
        newneighbor->NeighborIP = neighborIP;
    }

    if (key != NULL) {
        strncpy(newneighbor->key, key, sizeof(newneighbor->key)-1);
    }

    return newneighbor;
}

int cli_neighbor_help(int client_fd) {
    char msg[IPC_MESSAGE_SIZE] = { 0 };

    sprintf(msg,"Usage: [no] neighbor <ip address> [key]\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

struct commandresult cli_show_neighbors(int client_fd, char **parameters, int numparameters, void *data) {
    struct neighbor *currentneighbor = NULL;
    struct commandresult result  = {
                                       0
                                   };
    char temp[20];
    char col1[17];
    char col2[18];
    char col3[66];
    char end[3];
    char msg[IPC_MESSAGE_SIZE] = { 0 };

    currentneighbor = ipchead.next;

    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);
    sprintf(
        msg,
        "|   Neighbor IP   |       GUID       |                               Key                                |\n");
    cli_send_feedback(client_fd, msg);
    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);

    while(currentneighbor != NULL) {
        strcpy(msg, "");
        inet_ntop(AF_INET, &currentneighbor->NeighborIP, temp,
                  INET_ADDRSTRLEN);
        sprintf(col1, "| %-16s", temp);
        strcat(msg, col1);
        sprintf(col2, "| %-17s", currentneighbor->UUID);
        strcat(msg, col2);
        sprintf(col3, "| %-65s", currentneighbor->key);
        strcat(msg, col3);
        sprintf(end, "|\n");
        strcat(msg, end);
        cli_send_feedback(client_fd, msg);

        currentneighbor = currentneighbor->next;
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

int add_update_neighbor(int client_fd, __u32 neighborIP, char *key) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    /*
     * Make sure the neighbor does not already exist.
     */
    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            if(key != NULL) {
                strncpy(currentneighbor->key, key, sizeof(currentneighbor->key)-1);
            }

            return 0;
        }

        currentneighbor = currentneighbor->next;
    }

    /*
     * Did not find the neighbor so lets add it.
     */

    currentneighbor = allocate_neighbor(neighborIP, key);

    if (currentneighbor != NULL) {

        if (ipchead.next == NULL) {
            ipchead.next = currentneighbor;
            ipchead.prev = currentneighbor;

        } else {
            currentneighbor->prev = ipchead.prev;
            ipchead.prev->next = currentneighbor;
            ipchead.prev = currentneighbor;
        }

    }

    return 0;
}

int del_neighbor(int client_fd, __u32 neighborIP, char *key) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            if((currentneighbor->prev == NULL) && (currentneighbor->next == NULL)) {
                ipchead.next = NULL;
                ipchead.prev = NULL;

            } else if ((currentneighbor->prev == NULL) && (currentneighbor->next != NULL)) {
                ipchead.next = currentneighbor->next;
                currentneighbor->next->prev = NULL;

            } else if ((currentneighbor->prev != NULL) && (currentneighbor->next == NULL)) {
                ipchead.prev = currentneighbor->prev;
                currentneighbor->prev->next = NULL;

            } else if ((currentneighbor->next != NULL) && (currentneighbor->prev != NULL)) {
                currentneighbor->prev->next = currentneighbor->next;
                currentneighbor->next->prev = currentneighbor->prev;
            }

            free(currentneighbor);
            currentneighbor = NULL;

            return 0;
        }

        currentneighbor = currentneighbor->next;
    }

    return 0;
}

int validate_neighbor_input(int client_fd, char *stringip, char *key, t_neighbor_command neighbor_command) {
    int ERROR = 0;
    int keylength = 0;
    __u32 neighborIP = 0;
    char msg[IPC_MESSAGE_SIZE] = { 0 };

    /*
     * We must validate the user data here
     * before adding or updating anything.
     * 1. stringip should convert to an integer using inet_pton()
     * 2. key should be NULL or < 64 bytes
     */

    ERROR = inet_pton(AF_INET, stringip, &neighborIP); //Should return 1.

    if(ERROR != 1) {
        return cli_neighbor_help(client_fd);
    }

    if(key != NULL) {
        keylength = strlen(key);

        if(keylength > 64) {
            return cli_neighbor_help(client_fd);
        }
    }

    /*
     * The input is valid so we run the specified command
     * This will add, update or remove a neighbor.
     */
    neighbor_command(client_fd, neighborIP, key);

    /*
    sprintf(msg,"Neighbor string IP is [%s].\n", stringip);
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Neighbor integer IP is [%u].\n", ntohl(neighborIP));
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Neighbor key is [%s].\n", key);
    cli_send_feedback(client_fd, msg);

    if(key != NULL) {
        sprintf(msg,"Neighbor key length is [%u].\n", keylength);
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
struct commandresult cli_no_neighbor(int client_fd, char **parameters, int numparameters, void *data) {
    int ERROR = 0;
    struct commandresult result = {
                                      0
                                  };

    if (numparameters == 1) {
        ERROR = validate_neighbor_input(client_fd, parameters[0], NULL, &del_neighbor);

    } else {
        ERROR = cli_neighbor_help(client_fd);
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
struct commandresult cli_neighbor(int client_fd, char **parameters, int numparameters, void *data) {
    int socket = 0;
    int ERROR = 0;
    struct commandresult result  = {
                                       0
                                   };
    char buf[IPC_MESSAGE_SIZE];
    char message[LOGSZ] = {0};

    result.mode = NULL;
    result.data = NULL;

    if ((numparameters < 1) || (numparameters > 2)) {
        ERROR = cli_neighbor_help(client_fd);

    } else if (numparameters == 1) {
        ERROR = validate_neighbor_input(client_fd, parameters[0], NULL, &add_update_neighbor);

    } else if (numparameters == 2) {
        ERROR = validate_neighbor_input(client_fd, parameters[0], parameters[1], &add_update_neighbor);
    }

    socket = new_unix_client(OPENNOPD_IPC_SOCK);

    if (socket < 0) {
        sprintf(message, "IPC: CLI failed to connect.\n");
        logger(LOG_INFO, message);
        result.finished = 1;
        close(socket);
        return result;
    }

    sprintf(buf,"Hello!\n");
    ERROR = send(socket, buf, strlen(buf), 0);

    if (ERROR < 0) {
        sprintf(message, "IPC: CLI could not write to IPC socket.\n");
        logger(LOG_INFO, message);
        result.finished = 1;
        close(socket);
        return result;
    }
    ERROR = shutdown(socket, SHUT_WR);

    /*
     * TODO: Here we should read from the socket for x seconds.
     * If we receive the proper shutdown from the server we can close the socket.
     */

    result.finished = 0;

    return result;
}

/*
 * We need to verify that the remote accelerator is a member of this domain.
 * TODO: Later the UUID will need to be used instead of the IP address.
 */
int verify_neighbor_in_domain(__u32 neighborIP) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            return 1;
        }

        currentneighbor = currentneighbor->next;
    }

    return 0;
}

void start_ipc() {
    /*
     * This will setup and start the IPC threads.
     * We also register the IPC commands here.
     */
    register_command(NULL, "neighbor", cli_neighbor, true, false);
    register_command(NULL, "no neighbor", cli_no_neighbor, true, false);
    register_command(NULL, "show neighbors", cli_show_neighbors, false, false);

    ipchead.next = NULL;
    ipchead.prev = NULL;

    pthread_create(&t_ipc, NULL, ipc_thread, (void *) NULL);

}

void rejoin_ipc() {
    pthread_join(t_ipc, NULL);
}
