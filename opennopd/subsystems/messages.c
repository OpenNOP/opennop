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
struct cli_def *CLI;
struct cli_command *CLI_SHOW;
struct cli_command *CLI_DEBUG;

/*
 * Stores the PID of front end shell (opennop) that sent the command.
 * Used to make sure any output is sent back to the same front end shell.
 */
int CLI_SENDERPID;

int DEBUG_MESSAGES = false;

/*
 * This function enables/disables debug logging for the messages module.
 * It uses the standard cli_print() for sending output back to the user.
 * This however is redirected by the cli_print_callback() "print_cli_output".
 */
int cmd_debug_messages(struct cli_def *cli, const char *command, char *argv[],
		int argc) {

	switch (DEBUG_MESSAGES) {
	case true:
		DEBUG_MESSAGES = false;
		cli_print(cli, "debug messages disabled.");
		break;
	case false:
		DEBUG_MESSAGES = true;
		cli_print(cli, "debug messages enabled.");
		break;
	}
	return CLI_OK;
}

/* This function writes output back to the front end shell (opennop).
 * Its called by libcli when there is any output that needs sent to the screen.
 * When being called it uses the PID stored in CLI_SENDERPID.
 */
void print_cli_output(struct cli_def *cli, const char *theoutput) {
	int msqid;
	int msgflg = IPC_CREAT | 0666;
	key_t key;
	struct msgbuf sbuf;
	char message[LOGSZ];
	size_t buf_length;

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

	sbuf.mtype = CLI_SENDERPID; //When we use this it will be the PID of the CLI.
	strcpy(sbuf.mtext, theoutput);

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

/*
 * This is the main function for the messages/cli module & thread.
 * Main function for interacting with the front end shell (opennop).
 * This sets up and handles all IPC messages from the cli (opennop).
 */
void *messages_function(void *dummyPtr) {
	int msqid;
	int msgflg = IPC_CREAT | 0666;
	key_t key;
	struct msgbuf rbuf;
	char message[LOGSZ];
	size_t buf_length;

	//Setup CLI.
	CLI = cli_init();

	/*
	 * Register the cli callback.
	 * This redirects the normal cli_print() output to
	 * the front end shell (opennop).
	 */
	cli_print_callback(CLI, print_cli_output);

	//Register the global show & debug nodes.
	CLI_SHOW = cli_register_command(CLI, NULL, "show", NULL,
			PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
	CLI_DEBUG = cli_register_command(CLI, NULL, "debug", NULL,
			PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);

	//Register local debug messages node.  Calls cmd_debug_messages().
	struct cli_command *CLI_DEBUG_MESSAGES;
	CLI_DEBUG_MESSAGES = cli_register_command(CLI, CLI_DEBUG, "messages",
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
		 * Type 1 indicates a message originating from the front end shell(opennop)
		 * destined for the daemon (opennopd).
		 */
		if (msgrcv(msqid, &rbuf, buf_length, 1, 0) < 0) {

			if (DEBUG_MESSAGES == true) {
				sprintf(message, "Messages: Message received.");
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

		/*
		 * Save the sender PID.
		 * So any output generated by this command
		 * will be sent back to the correct front end shell (opennop).
		 */
		CLI_SENDERPID = rbuf.sender;

		//Run the command through the libcli parser.
		cli_run_command(CLI, rbuf.mtext);
	}
	return NULL;
}
