#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // for sleep function
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <linux/types.h>

#include "messages.h"
#include "opennopd.h"
#include "logger.h"
#include "libcli.h"

//Global root nodes for CLI.
struct cli_command *CLI_SHOW;
struct cli_command *CLI_DEBUG;
struct cli_def *cli;
int DEBUG_MESSAGES = false;

//This function enables/disables debug logging for the messages module.
int cmd_debug_messages(struct cli_def *cli, char *command, char *argv[],
		int argc) {

	switch (DEBUG_MESSAGES) {
	case true:
		DEBUG_MESSAGES = false;
		break;
	case false:
		DEBUG_MESSAGES = true;
		break;
	}
	return CLI_OK;
}

void *messages_function(void *dummyPtr) {
	int msqid;
	int msgflg = IPC_CREAT | 0666;
	key_t key;
	struct msgbuf rbuf;
	struct msgbuf sbuf;
	char message[LOGSZ];
	size_t buf_length;
	int stringcompare;

	//Setup CLI.
	cli = cli_init();

	//Register show & debug nodes.
	CLI_SHOW = cli_register_command(cli, NULL, "show", NULL,
			PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
	CLI_DEBUG = cli_register_command(cli, NULL, "debug", NULL,
			PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);

	//Register local debug node.  Calls cmd_debug_messages().
	struct cli_command *CLI_DEBUG_MESSAGES;
	CLI_DEBUG_MESSAGES = cli_register_command(cli, CLI_DEBUG, "messages",
			cmd_debug_messages, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);

	/*
	 * Get the message queue id for the
	 * "name" 1234, which was created by
	 * the server.
	 */
	if ((key = ftok("/var/opennop", 'o')) == -1) { /* Create the opennop key */

		if (DEBUG_MESSAGES == true) {
			sprintf(message, "Could not create the opennop message queue key.");
			logger(LOG_INFO, message);
			sprintf(message, "Does /var/opennop exist?");
			logger(LOG_INFO, message);
		}
		exit(1);
	}

	if (DEBUG_MESSAGES == true) {
		sprintf(message, "\nmsgget: Calling msgget(%#x, %#o)\n", key, msgflg);
		logger(LOG_INFO, message);
	}

	if ((msqid = msgget(key, msgflg)) == -1) { /* Connect to the message queue */

		if (DEBUG_MESSAGES == true) {
			sprintf(message, "Could not connect to the message queue.");
			logger(LOG_INFO, message);
		}
		exit(1);
	} else {

		if (DEBUG_MESSAGES == true) {
			sprintf(message, "msgget: msgget succeeded: msqid = %d\n", msqid);
			logger(LOG_INFO, message);
		}
	}

	buf_length = sizeof(struct msgbuf) - sizeof(long);

	while (servicestate >= STOPPING) {

		/*
		 * This thread should run until the parent thread
		 * has reached a "SHUTDOWN" state.
		 *
		 * This is where the interface to send and receive
		 * command line events/messages should be processed.
		 */

		/*
		 * Receive an answer of message type 1.
		 */
		if (msgrcv(msqid, &rbuf, buf_length, 1, 0) < 0) {

			if (DEBUG_MESSAGES == true) {
				sprintf(message, "msgrcv");
				logger(LOG_INFO, message);
			}
			exit(1);
		}

		/*
		 * Print the answer.
		 */
		if (DEBUG_MESSAGES == true) {
			sprintf(message, "%i,%s\n", rbuf.sender, rbuf.mtext);
			logger(LOG_INFO, message);
		}

		//Run the command through the libcli parser.
		cli_run_command(cli, rbuf.mtext);

		if (stringcompare == 0) {


			/*
			 * TODO: Send a message back to the CLI.
			 * This needs to be converted to a function.
			 * It will be used a lot by different modules,
			 * to send output back to the CLI.
			 * This will need registered with each command.
			 */

			sbuf.mtype = rbuf.sender; //TWhen we use this it will be the PID of the CLI.
			strcpy(sbuf.mtext, "OK\n");

			if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {

				if (DEBUG_MESSAGES == true) {
					sprintf(message, "Error sending message.");
					logger(LOG_INFO, message);
				}
			} else {

				if (DEBUG_MESSAGES == true) {
					sprintf(message, "Message: \"%s\" Sent\n", sbuf.mtext);
					logger(LOG_INFO, message);
				}
			}

		}

	}
	return NULL;
}
