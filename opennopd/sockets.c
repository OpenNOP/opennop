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
#include "logger.h"
#include "sockets.h"

#define MAXEVENTS 64

int new_ip_client(__u32 serverip ,int port) {
    int client_socket = 0;
    int error = 0;
    struct sockaddr_in client = {
                                    0
                                };
    char message[LOGSZ];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0 ) {
        sprintf(message, "IPC: Failed to create socket.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    client.sin_addr.s_addr = serverip;

    error = connect(client_socket, (struct sockaddr *)&client, sizeof(client));

    if (error < 0) {
        sprintf(message, "IPC: Failed to connect to remote host.\n");
        logger(LOG_INFO, message);
        return -1;
    }

    return client_socket;
}
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
        logger(LOG_INFO, message);
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


int epoll_handler(struct epoll_server *server){
	int client_socket;
    int error = 0;
    int numevents = 0;
    int done = 0;
    int i = 0;
    ssize_t count;
    char message[LOGSZ] = {0};
    char buf[IPC_MAX_MESSAGE_SIZE];

    /*
     * Third we listen for events and handle them.
     */
    while(1) {
        numevents = epoll_wait(server->epoll_fd, server->events, MAXEVENTS, 10000);

        for (i = 0; i < numevents; i++) {

            if ((server->events[i].events & EPOLLERR) ||
                    (server->events[i].events & EPOLLHUP) ||
                    (!(server->events[i].events & EPOLLIN))) {
                /*
                 * An error has occurred on this fd, or the socket is not
                 * ready for reading (why were we notified then?)
                 */

                sprintf(message, "IPC: Epoll error.\n");
                logger(LOG_INFO, message);

                close (server->events[i].data.fd);

                continue;

            } else if (server->events[i].data.fd == server->socket) {
                /*
                 * We have a notification on the listening socket,
                 * which means one or more incoming connections.
                 */
                while (1) {
                	/*
                	 * TODO:
                	 * Here we need to detect if the "server" was UNIX or IP.
                	 * We might use the family type and add it to "struct epoll_server"
                	 * and set it when we create the new epoll server.
                	 */
                    client_socket = accept_ip_client(server->socket);

                    if (client_socket == -1) {
                        break;
                    }

                    error = make_socket_non_blocking(client_socket);

                    if (error == -1) {
                        sprintf(message, "IPC: Failed setting socket on client.\n");
                        logger(LOG_INFO, message);
                        exit(1);
                    }

                    error = register_socket(client_socket, server->epoll_fd, &server->event);

                    continue;
                }

            }  else {
                /*
                 * We have data on the fd waiting to be read. Read and
                 * display it. We must read whatever data is available
                    * completely, as we are running in edge-triggered mode
                    * and won't get a notification again for the same
                    * data.
                 */

            	done = 0;  //Need to reset this for each message.
                while(1) {
                    count = recv(server->events[i].data.fd, buf, IPC_MAX_MESSAGE_SIZE, 0);
                    /*
                     * Third we listen for events and handle them.
                     */
                    while(1) {
                        numevents = epoll_wait(server->epoll_fd, server->events, MAXEVENTS, 0);

                        for (i = 0; i < numevents; i++) {

                            if ((server->events[i].events & EPOLLERR) ||
                                    (server->events[i].events & EPOLLHUP) ||
                                    (!(server->events[i].events & EPOLLIN))) {
                                /*
                                 * An error has occurred on this fd, or the socket is not
                                 * ready for reading (why were we notified then?)
                                 */

                                sprintf(message, "IPC: Epoll error.\n");
                                logger(LOG_INFO, message);

                                close (server->events[i].data.fd);

                                continue;

                            } else if (server->events[i].data.fd == server->socket) {
                                /*
                                 * We have a notification on the listening socket,
                                 * which means one or more incoming connections.
                                 */
                                while (1) {
                                    client_socket = accept_ip_client(server->socket);

                                    if (client_socket == -1) {
                                        break;
                                    }

                                    error = make_socket_non_blocking(client_socket);

                                    if (error == -1) {
                                        sprintf(message, "IPC: Failed setting socket on client.\n");
                                        logger(LOG_INFO, message);
                                        exit(1);
                                    }

                                    error = register_socket(client_socket, server->epoll_fd, &server->event);

                                    continue;
                                }

                            }  else {
                                /*
                                 * We have data on the fd waiting to be read. Read and
                                 * display it. We must read whatever data is available
                                    * completely, as we are running in edge-triggered mode
                                    * and won't get a notification again for the same
                                    * data.
                                 */

                            	done = 0;  //Need to reset this for each message.
                                while(1) {
                                    count = recv(server->events[i].data.fd, buf, IPC_MAX_MESSAGE_SIZE, 0);

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
                                        		server->events[i].data.fd);
                                        logger(LOG_INFO, message);

                                        /*
                                         * Closing the descriptor will make epoll remove it
                                         * from the set of descriptors which are monitored.
                                         */
                                        close(server->events[i].data.fd);
                                    }
                                }
                            }
                        }

                        /*
                         * TODO:
                         * Here is where we can execute other tasks.
                         * I think this needs moved to a tasks thread.
                         * There is no point in it being here.
                         */
                        //hello_neighbors();
                        (server->callback());
                    }
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
                        		server->events[i].data.fd);
                        logger(LOG_INFO, message);

                        /*
                         * Closing the descriptor will make epoll remove it
                         * from the set of descriptors which are monitored.
                         */
                        close(server->events[i].data.fd);
                    }
                }
            }
        }

        /*
         * TODO:
         * Here is where we can execute other tasks.
         * I think this needs moved to a tasks thread.
         * There is no point in it being here.
         */
        //hello_neighbors();
        (server->callback());
    }
    return 0;
}

int new_ip_epoll_server(struct epoll_server *server, int (*callback)(void), int port){
	char message[LOGSZ] = {0};

	server->events = calloc (MAXEVENTS, sizeof server->event);

	if(server->events == NULL){
		exit(1);
	}

	server->epoll_fd = epoll_create1(0);

    if(server->epoll_fd == -1) {
        sprintf(message, "IPC: Could not create epoll instance.\n");
        logger(LOG_INFO, message);
        exit(1);
    }

    /*
     * First we setup the remote IPC listener socket.
     * This accepts connections from the remote neighbors.
     */

    server->socket = new_ip_server(OPENNOPD_IPC_PORT);

    register_socket(server->socket, server->epoll_fd, &server->event);

    server->callback = callback;

	return 0;
}


int shutdown_epoll_server(struct epoll_server *server){
    free(server->events);
    close(server->socket);
    return 0;
}
