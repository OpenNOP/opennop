#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
//#include <curses.h>
#include <pthread.h> // for multi-threading
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#include "messages.h"

int main()
{

    bool quit = false; // used to exit when "quit" is entered.
    char text[MSGSZ];
    int stringcompare;

    /*
     * Varibles for message queue.
     */
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    struct msgbuf sbuf;
    size_t buf_length;

    /*
     * Get the message queue id for the
     * "name" 1234, which was created by
     * the server.
     */
    if ((key = ftok("/var/opennop",'o')) == -1)
    { /* Create the opennop key */
        fprintf(stderr,"Could not create the opennop message queue key.");
        fprintf(stderr,"Does /var/opennop exist?");
        exit(1);
    }

    fprintf(stderr, "\nmsgget: Calling msgget(%#x,\%#o)\n",key, msgflg);

    /*
     * Get a message ID from the queue.?
     */
    if ((msqid = msgget(key, msgflg )) < 0)
    {
        perror("msgget");
        exit(1);
    }
    else
    {
        fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);
    }

    /*
     * We'll send message type 1
     */

    sbuf.mtype = 1;

    fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);

    fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);

    buf_length = sizeof(struct msgbuf) - sizeof(long);

    pthread_t t_message; // thread for passing messages.

    fprintf(stderr,"opennop: Starting listener thread.\n");
    pthread_create(&t_message, NULL, messages_function, (void *)NULL);

    while (quit != true)
    {

        fputs("enter some text: ", stdout);
        fflush(stdout); /* http://c-faq.com/_stdio/fflush.html */

        if (fgets(text, sizeof text, stdin) != NULL)
        {
            char *newline = strchr(text, '\n'); /* search for newline character */

            if (newline != NULL)
            {
                *newline = '\0'; /* overwrite trailing newline */
            }
            printf("text = \"%s\"\n", text);
            stringcompare = strncmp(text, "quit", MSGSZ);

            if (stringcompare == 0)
            {
                quit = true;
            }
            else
            {

                strcpy(sbuf.mtext, text);
                sbuf.sender = getpid();

                /*
                 * Send a message.
                 */
                if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0)
                {
                    printf("Message Queue ID: %i\n", msqid);
                    printf("Message Type: %li\n", sbuf.mtype);
                    printf("Message Text: %s\n", sbuf.mtext);
                    printf("Message Length: %u\n", (uint)buf_length);
                    perror("msgsnd\n");
                    exit(1);
                }
                else
                {
                    printf("Message: \"%s\" Sent\n", sbuf.mtext);
                }
            }
        }
    }

    /*
     * I dont want to wait for the thread to end just stop it.
     */
    //pthread_join(t_messages, NULL);

    return 0;
}



