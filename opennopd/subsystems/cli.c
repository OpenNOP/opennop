#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for sleep function

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <linux/types.h>

#include "../../include/cli.h"
#include "../../include/opennopd.h"
#include "../../include/logger.h"




void *cli_function (void *dummyPtr)
{
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    message_buf rbuf;
    message_buf sbuf;
    char message [LOGSZ];
    size_t buf_length;
    int stringcompare;

    /*
     * Get the message queue id for the
     * "name" 1234, which was created by
     * the server.
     */
    if ((key = ftok("/var/opennop",'o')) == -1)
    { /* Create the opennop key */
        sprintf(message, "Could not create the opennop message queue key.");
        logger(LOG_INFO, message);
        sprintf(message, "Does /var/opennop exist?");
        logger(LOG_INFO, message);
        exit(1);
    }

    sprintf(message, "\nmsgget: Calling msgget(%#x, %#o)\n", key, msgflg);
    logger(LOG_INFO, message);

    if ((msqid = msgget(key, msgflg )) == -1)
    { /* Connect to the message queue */
        sprintf(message, "Could not connect to the message queue.");
        logger(LOG_INFO, message);
        exit(1);
    }
    else
    {
        sprintf(message,"msgget: msgget succeeded: msqid = %d\n", msqid);
        logger(LOG_INFO, message);
    }

    buf_length = sizeof(message_buf) - sizeof(long);

    while (servicestate >= STOPPING)
    {

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
        if (msgrcv(msqid, &rbuf, buf_length, 1, 0) < 0)
        {
            sprintf(message,"msgrcv");
            logger(LOG_INFO, message);
            exit(1);
        }

        /*
         * Print the answer.
         */
        sprintf(message,"%i,%s\n",rbuf.sender, rbuf.mtext);
        logger(LOG_INFO, message);


        /*
         * I want to know when the user enters "show" in the CLI.
         * Then send an "OK" message back to the CLI.
         * Ill need to develop a structure that will be used,
         * to pass data back/forth this is started in the cli.h
         * the name might change later.  The framework of how
         * CLI command will be read is next.
         */
        stringcompare = strncmp(rbuf.mtext, "show", MSGSZ);
        if (stringcompare == 0)
        {

            /*
             * Send a message back to the CLI.
             * I think this should be a function.
             * It will be used a lot by different modules,
             * to send output back to the CLI.
             */

            sbuf.mtype = rbuf.sender; //TWhen we use this it will be the PID of the CLI.
            strcpy(sbuf.mtext, "OK\n");

            if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0)
            {
                sprintf(message, "Error sending message.");
                logger(LOG_INFO, message);
            }
            else
            {
                sprintf(message, "Message: \"%s\" Sent\n", sbuf.mtext);
                logger(LOG_INFO, message);
            }

        }


    }
    return NULL;
}
