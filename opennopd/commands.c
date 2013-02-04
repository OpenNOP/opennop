#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/types.h>

#include <linux/types.h>

#include "commands.h"

/* Structure used for the head of a command list.. */
struct command_head
{
    struct command *next; // Points to the first command of the list.
    struct command *prev; // Points to the last command of the list.
    u_int32_t qlen; // Total number of command functions registererd to the list.
    pthread_mutex_t lock; // Lock for this queue.
};

struct command
{
    struct command_head *head; // Points to the head of this list.
    struct command *next; // Points to the next command.
    struct command *prev; // Points to the previous command.
    int (*module_commandfunction)(struct msgbuf);
};

int DEBUG_COMMANDS = false;
struct command_head commands; //Head for list of command functions.

int registercommandfunction(int (*commandfunction)(struct msgbuf))
{

    /**
     * @todo Allocate a new "command".
     * @todo Add it to the command_head list.
     */

    return 0;
}
