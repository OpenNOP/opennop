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
