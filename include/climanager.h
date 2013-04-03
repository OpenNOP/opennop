#ifndef	_CLI_MANAGER_H_
#define	_CLI_MANAGER_H_

#include "clicommands.h"

#define	MAX_BUFFER_SIZE	1024

int cli_process_message(int client_fd, char *buffer, int d_len);
void start_cli_server();
int cli_help();
void cli_manager_init();
int cli_send_feedback(char *msg);


#endif
