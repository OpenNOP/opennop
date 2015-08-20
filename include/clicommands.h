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
    char prompt[255]; // Prompt for this command node.
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

struct commandresult execute_commands(struct command_head *mode, void *data, int client_fd, const char *command_name, int d_len);
int register_command(struct command_head *mode, const char *command_name, t_commandfunction handler_function, bool, bool);
int cli_prompt(int client_fd);
int cli_mode_prompt(int client_fd, struct command_head *mode);
void bytestostringbps(char *output, __u32 count);
int cli_send_feedback(int client_fd, char *msg);
void init_cli_global_mode();
void initializetestmode();

#endif
