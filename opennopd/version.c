#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "clicommands.h"
#include "version.h"
#include "logger.h"

struct commandresult cli_show_version(int client_fd, char **parameters, int numparameters, void *data) {
    struct commandresult result = { 0 };
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg, "Version %s.\n", VERSION);
    cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}
