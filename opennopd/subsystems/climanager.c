#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h> // for multi-threading
#include <sys/socket.h>
#include <sys/un.h>

#include <linux/types.h>

#include "climanager.h"
#include "clicommands.h"
#include "clisocket.h"
#include "logger.h"

/*
 * Starts the CLI socket.
 * http://troydhanson.github.io/misc/Unix_domain_sockets.html
 */
void start_cli_server() {
    int socket_desc, client_sock, c, *new_sock, length;
    struct sockaddr_un server, client;

    //Create socket
    socket_desc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }
    puts("Socket created");

    //Prepare the sockaddr_in structure
    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, OPENNOP_SOCK, sizeof(server.sun_path)-1);
    unlink(server.sun_path);

    //Bind the socket
    length = strlen(server.sun_path) + sizeof(server.sun_family);

    if (bind(socket_desc, (struct sockaddr *) &server, length) < 0) {
        //print the error message
        perror("bind failed. Error");
        exit(1);
    }
    puts("bind done");

    //Listen
    if (listen(socket_desc, 1) == -1) {
        perror("[cli_manager]:Listen");
        exit(1);
    }

    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_un);

    while ((client_sock = accept(socket_desc, (struct sockaddr *) &client,
                                 (socklen_t*) &c))) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        if (pthread_create(&sniffer_thread, NULL, client_handler,
                           (void*) new_sock) < 0) {
            perror("could not create thread");
            exit(1);
        }

        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }

    if (client_sock < 0) {
        perror("accept failed");
        exit(1);
    }

    close(socket_desc);

    return;
}
void *client_handler(void *socket_desc) {
    //Get the socket descriptor
    int sock = *(int*) socket_desc;
    int read_size, finish = 0;
    struct commandresult result = { 0 };
    struct command_head *mode = NULL; // stores the mode command_head.
    void *data = NULL; // Used to store custom data for configuration mode.
    char client_message[MAX_BUFFER_SIZE];
    char message[LOGSZ];

    sprintf(message, "Started cli connection.\n");
    logger(LOG_INFO, message);

    cli_prompt(sock);

    //Receive a message from client
    while (!(finish) && ((read_size = recv(sock, client_message,
                                           MAX_BUFFER_SIZE, 0)) > 0)) {

        client_message[read_size - 1] = '\0';

        if (read_size) {

            result = execute_commands(mode, data, sock, client_message, read_size);
            mode = result.mode;
            data = result.data;
        }

        if (result.finished) {
        	close(sock);
        }

        if (read_size == 0) {
            puts("Client disconnected");
            fflush(stdout);
        } else if (read_size == -1) {
            perror("recv failed");
        }
    }
    //Free the socket pointer
    free(socket_desc);

    sprintf(message, "Closing cli connection.\n");
    logger(LOG_INFO, message);

    return NULL;
}

struct commandresult cli_quit(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };
    /*
     * Received a quit command so return 1 to shutdown this cli session.
     */
    result.finished = 1;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

void *cli_manager_init(void *dummyPtr) {
    register_command(NULL, "quit", cli_quit, false, false);
    register_command(NULL, "exit", cli_quit, false, true);
    register_command(NULL, "end", cli_quit, false, true);
    initializetestmode();
    //register_command(NULL, "show parameters", cli_show_params, true, true);

    /* Sharwan Joram:t We'll call this in last as we will never return from here */
    start_cli_server();
    return NULL;
}

