#ifndef	_CLI_MANAGER_H_
#define	_CLI_MANAGER_H_

#include "clicommands.h"
#include "clisocket.h"

int cli_process_message(int client_fd, char *buffer, int d_len);
void start_cli_server();
int cli_help();
void cli_manager_init();
int cli_send_feedback(int client_fd, char *msg);
void bytestostringbps(char *output, __u32 count);
void *client_handler(void *socket_desc);

#endif
