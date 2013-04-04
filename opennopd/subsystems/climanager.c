#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "climanager.h"
#include "clicommands.h"

#define	OPENNOP_SOCK	"/tmp/opennop.sock"

int server_fd;
int client_fd;

int cli_process_message(int client_fd, char *buffer, int d_len) {
	struct cli_commands *cmd;
	char msg[128] = { 0 };
	if (0 == (cmd = lookup_command(buffer))) {
		fprintf(stdout, "There is no such command \n");
		memcpy(msg, "Invalid Command...", 28);
		if (!(send(client_fd, msg, strlen(msg), 0))) {
			perror("[cli_manager]: Send");
			exit(1);
		}
		return 0;
	} else {
		(cmd->command_handler)();
	}
	return 0;
}

void start_cli_server() {
	int length;
	char sock_buffer[MAX_BUFFER_SIZE];
	struct sockaddr_un server, client;

	if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("[cli_manager]: socket");
		exit(1);
	}

	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, OPENNOP_SOCK);
	unlink(server.sun_path);

	length = strlen(server.sun_path) + sizeof(server.sun_family);

	if (bind(server_fd, (struct sockaddr *) &server, length) == -1) {
		perror("[cli_manager]: Bind");
		exit(1);
	}

	if (listen(server_fd, 1) == -1) {
		perror("[cli_manager]:Listen");
		exit(1);
	}

	while (1) {
		int finish = 0, data_length;

		data_length = sizeof(client);
		if ((client_fd = accept(server_fd, (struct sockaddr *) &client,
				&data_length)) == -1) {
			perror("[cli_manager]:Accept");
			exit(1);
		}

		while (!(finish) && (client_fd)) {
			data_length = recv(client_fd, sock_buffer, sizeof(sock_buffer), 0);
			sock_buffer[data_length - 1] = '\0';
			if (data_length)
				finish = cli_process_message(client_fd, sock_buffer,
						data_length);
		}

		if (finish)
			shutdown(client_fd, SHUT_RDWR);
	}
}
int cli_quit() {
	char msg[24] = { 0 };
	strcpy(msg, "....BYE....\n");

	if ((send(client_fd, msg, strlen(msg), 0)) <= 0) {
		perror("[cli_manager]: send");
		exit(1);
	}

	shutdown(client_fd, SHUT_RDWR);
	client_fd = 0;
	return 1;
}

int cli_help() {
	char msg[MAX_BUFFER_SIZE] = { 0 };
	char cmd[36] = { 0 };
	struct cli_commands *itr_node;
	int count = 1;

	itr_node = head;
	strcat(msg, "\n Available command list are : \n");

	while (itr_node) {
		sprintf(msg, "[%d]: [%s] \n", count, itr_node->command);
		cli_send_feedback(msg);
		itr_node = itr_node->next;
		++count;
	}

	return 0;
}

void cli_manager_init() {
	register_command("help", cli_help);
	register_command("quit", cli_quit);

	/* Sharwan Joram:t We'll call this in last as we will never return from here */
	start_cli_server();

}

int cli_send_feedback(char *msg) {
	if ((send(client_fd, msg, strlen(msg), 0)) <= 0) {
		perror("[cli_manager]: send");
		return 1;
	}
	return 0;
}
