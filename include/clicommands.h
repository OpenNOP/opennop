#ifndef	_CLI_COMMANDS_H_
#define	_CLI_COMMANDS_H_


struct cli_commands {
	char *command;
	int (*command_handler)(int client_fd);
	struct cli_commands *next;
	struct cli_commands *prev;
};


struct cli_commands *head;
struct cli_commands *end;

struct cli_commands* lookup_command(const char *command_name);
int register_command(const char *command_name, int (*handler_function)(int client_fd));

#endif
