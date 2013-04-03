#ifndef	_CLI_MANAGER_H_
#define	_CLI_MANAGER_H_

#include "clicommands.h"


int cli_process_message(int client_fd, char *buffer, int d_len);
void start_cli_server();
int cli_help();
void cli_manager_init();


#endif
