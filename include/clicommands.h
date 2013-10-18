#ifndef	_CLI_COMMANDS_H_
#define	_CLI_COMMANDS_H_

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

#include <linux/types.h>

#include "clisocket.h"

typedef struct commandresult (*t_commandfunction)(int, char **, int, void *);

struct command_head {
    struct command *next; // Points to the first command of the list.
    struct command *prev; // Points to the last command of the list.
    pthread_mutex_t lock; // Lock for this node.
};

struct command {
    char *command;
    t_commandfunction command_handler;
    struct command *next;
    struct command *prev;
    struct command_head child;
    bool hasparams;
    bool hidden;
};

struct commandresult {
    int finished; // this is typically 0|1 (0=don't exit cli, 1=exit cli)
    struct command_head *mode; // The command list for this mode.
    void *data; // Pointer to any data needed to retain context.
};

struct command* lookup_command(const char *command_name);
struct commandresult execute_commands(struct command_head *mode, void *data, int client_fd, const char *command_name, int d_len);
int register_command(struct command_head *mode, const char *command_name, t_commandfunction handler_function, bool, bool);
struct command* find_command(struct command_head *node, char *command_name);
int cli_prompt(int client_fd);
int cli_help(int client_fd, struct command_head *currentnode);
struct commandresult cli_show_params(int client_fd, char **parameters, int numparameters, void *data);
void bytestostringbps(char *output, __u32 count);
int cli_send_feedback(int client_fd, char *msg);
void initializetestmode();

#endif
