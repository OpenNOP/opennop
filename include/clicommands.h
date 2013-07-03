#ifndef	_CLI_COMMANDS_H_
#define	_CLI_COMMANDS_H_

#include <stdint.h>
#include <pthread.h>

struct command_head
{
    struct command *next; // Points to the first command of the list.
    struct command *prev; // Points to the last command of the list.
};

struct command {
	char *command;
	int (*command_handler)(int, char *);
	struct command *next;
	struct command *prev;
	struct command_head child;
	int hasparams;
};

int cli_help();
struct command* lookup_command(const char *command_name);
int execute_commands(int client_fd, const char *command_name, int d_len);
int register_command(const char *command_name, int (*handler_function)(int, char *));
struct command* find_command(struct command_head *node, char *command_name);

#endif
