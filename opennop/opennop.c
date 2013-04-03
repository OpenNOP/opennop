#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define	OPENNOP_SOCK	"/tmp/opennop.sock"
#define MAX_BUFFER_SIZE	1024

int client_fd;


int main(void)
{
	int length;
	struct sockaddr_un server;
	char sock_buffer[MAX_BUFFER_SIZE] = {0};

	if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ){
		perror("[cli_client]: socket");
		exit(1);
	}

	fprintf(stdout, "\n Connecting to OpenNOP CLI... \n");
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, OPENNOP_SOCK);
	length = strlen(server.sun_path) + sizeof(server.sun_family);

	if ( connect(client_fd, (struct sockaddr *)&server, length) == -1){
		perror("[cli_client]: Connect");
		exit(1);
	}

	fprintf(stdout, " Connected.....\n");
	
	while(printf("\n OpenNOPD >> ")){
		fgets(sock_buffer, 100, stdin);
		if (send(client_fd, sock_buffer, strlen(sock_buffer), 0) == -1){
			perror("[cli_client]: send");
			exit(1);
		}

		if ((length = recv(client_fd, sock_buffer, MAX_BUFFER_SIZE, 0)) > 0) {
			sock_buffer[length] = '\0';
			fprintf(stdout, "%s", sock_buffer);
		} else {
			if (length < 0)
				perror("[cli_client]: recv");
			else
				fprintf(stdout, "server closed connection \n");
			exit(1);
		}
	}

	close(client_fd);
	return 0;
}

