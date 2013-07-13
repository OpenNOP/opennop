#ifndef	_CLI_MANAGER_H_
#define	_CLI_MANAGER_H_

#include <stdint.h>

#include <linux/types.h>

#include "clicommands.h"
#include "clisocket.h"

int cli_process_message(int client_fd, char *buffer, int d_len);
void start_cli_server();
void cli_manager_init();
void *client_handler(void *socket_desc);

#endif
