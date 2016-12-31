#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h> // for multi-threading
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "clisocket.h"

void *fromserver_handler(void *dummyPtr) {
	int client_fd = *(int*)dummyPtr;
	int length;
	char server_reply[MAX_BUFFER_SIZE] = { 0 };

	while (((length = recv(client_fd, server_reply, MAX_BUFFER_SIZE, 0)) >= 0)
			&& (client_fd != 0)) {

		if (length > 0) {
			server_reply[length] = '\0';
			fprintf(stdout, "%s", server_reply);
			fflush(stdout);
		} else {
			if (length < 0)
				perror("[cli_client]: recv");
			else
				//fprintf(stdout, "server closed connection\n");
				break;
			//exit(1);
		}
	}
	return 0;
}

int main(void) {
	int client_fd;
	int length;
	struct sockaddr_un server;
	pthread_t t_fromserver;
	char* input;

	if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("[cli_client]: socket");
		exit(1);
	}

	//fprintf(stdout, "\n Connecting to OpenNOP CLI... \n");
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, OPENNOP_SOCK);
	length = strlen(server.sun_path) + sizeof(server.sun_family);

	if (connect(client_fd, (struct sockaddr *) &server, length) == -1) {
		perror("[cli_client]: Connect");
		exit(1);
	}

	//fprintf(stdout, " Connected.....\n");

	pthread_create(&t_fromserver, NULL, fromserver_handler, (void *)&client_fd);

	// printf("\n opennopd# ");

	rl_bind_key('\t',rl_abort);//disable auto-complet

	while (2) {

		input = readline("");

 		if (!input) {
			break;		
		}

		add_history(input);

		strcat(input,"\n");;

		if (send(client_fd, input, strlen(input), 0) == -1) {
			perror("[cli_client]: send");
			//exit(1);
			free(input);
			break;
		}

		free(input);

	}

	pthread_join(t_fromserver, NULL);

	close(client_fd);
	return 0;
}

