#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for sleep function

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <linux/types.h>

#include "../../include/messages.h"

void *messages_function (void *dummyPtr)
{
    pid_t pid;
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    struct msgbuf rbuf;
    size_t buf_length;

    /*
     * Get the PID of the process.
     * Used to receive messages back from the daemon.
     */
    pid = getpid();

    /*
     * Get the message queue id for the
     * "name" 1234, which was created by
     * the server.
     */
    if ((key = ftok("/var/opennop",'o')) == -1)
    { /* Create the opennop key */
        printf( "Could not create the opennop message queue key.");
        printf("Does /var/opennop exist?\n");
        exit(1);
    }

    printf("\nmsgget: Calling msgget(%#x, %#o)\n", key, msgflg);

    if ((msqid = msgget(key, msgflg )) == -1)
    { /* Connect to the message queue */
        printf("Could not connect to the message queue.");
        exit(1);
    }
    else
    {
        printf("msgget: msgget succeeded: msqid = %d\n", msqid);
    }

    buf_length = sizeof(struct msgbuf) - sizeof(long);

    for (;;)
    {

        /*
         * This thread should run until the parent thread
         * has reached a "SHUTDOWN" state.
         * 
         * This is where the interface to send and receive 
         * command line events/messages should be processed.
         */


        /*
         * Receive an answer of message type equal to the PID.
         */
        if (msgrcv(msqid, &rbuf, buf_length, pid, 0) < 0)
        {
            perror("msgrcv");
            exit(1);
        }

        /*
         * Print the answer.
         */
        printf("%s\n", rbuf.mtext);
    }
    return NULL;
}
