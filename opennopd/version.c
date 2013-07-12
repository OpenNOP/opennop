#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "clicommands.h"
#include "version.h"
#include "logger.h"

int cli_show_version(int client_fd, char **parameters, int numparameters) {
	char msg[MAX_BUFFER_SIZE] = { 0 };

	sprintf(msg, "Version %s.\n", VERSION);
	cli_prompt(client_fd);
	cli_send_feedback(client_fd, msg);
	cli_prompt(client_fd);

	return 0;
}
