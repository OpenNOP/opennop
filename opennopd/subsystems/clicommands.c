#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clicommands.h"

int register_command(const char *command_name, int (*handler_function)())
{
	struct cli_commands *node = (struct cli_commands *) malloc (sizeof (struct cli_commands));

	if (NULL == node){
		fprintf(stdout, "Could not allocate memory... \n");
		exit(1);
	}

	if (NULL == head){
		node->command = command_name;
		node->command_handler = handler_function;
		node->next = NULL;
		node->prev = NULL;
		head = node;
		end = head;
	} else {
		node->command = command_name;
		node->command_handler = handler_function;
		end->next = node;
		node->prev = end;
		end = node;
	}

	fprintf(stdout, "[%s]: command [%s] registered \n", __FUNCTION__, node->command);
	return 0;
}

struct cli_commands* lookup_command(const char *command_name)
{
	struct cli_commands *trav_node;
	trav_node = head;

	while(trav_node != NULL ){
		if (!strcmp(trav_node->command, command_name))
			return trav_node;
		trav_node = trav_node->next;
	}
	
	/* Sharwan Joram: If we are here, then we didn't find any command with us */
	return NULL;
}

