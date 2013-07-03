#ifndef	_CLI_COMMANDS_H_
#define	_CLI_COMMANDS_H_

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

struct command_head
{
    struct command *next; // Points to the first command of the list.
    struct command *prev; // Points to the last command of the list.
    pthread_mutex_t lock; // Lock for this node.
};

struct command {
	char *command;
	int (*command_handler)(int, char *);
	struct command *next;
	struct command *prev;
	struct command_head child;
	bool hasparams;
	bool hidden;
};

int cli_help();
struct command* lookup_command(const char *command_name);
int execute_commands(int client_fd, const char *command_name, int d_len);
int register_command(const char *command_name, int (*handler_function)(int, char *),bool,bool);
struct command* find_command(struct command_head *node, char *command_name);
int cli_prompt(int client_fd);
int cli_node_help(int client_fd, struct command_head *currentnode);
int cli_show_param(int client_fd, char *args);

#endif
