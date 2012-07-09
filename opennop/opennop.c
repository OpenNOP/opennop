#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>

#include "../include/cli.h"

int main()
{
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    message_buf sbuf;
    size_t buf_length;

    /*
     * Get the message queue id for the
     * "name" 1234, which was created by
     * the server.
     */
    if ((key = ftok("/var/opennop",'o')) == -1) { /* Create the opennop key */
		fprintf(stderr,"Could not create the opennop message queue key.");
        fprintf(stderr,"Does /var/opennop exist?");
        exit(1);
	}

(void) fprintf(stderr, "\nmsgget: Calling msgget(%#x,\%#o)\n",key, msgflg);
	
	/*
	 * Get a message ID from the queue.?
	 */
    if ((msqid = msgget(key, msgflg )) < 0) {
        perror("msgget");
        exit(1);
    }
    else 
     (void) fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);


    /*
     * We'll send message type 1
     */
     
    sbuf.mtype = 1;
    
    (void) fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);
    
    (void) strcpy(sbuf.mtext, "Did you get this?\n");
    
    (void) fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);
    
    buf_length = strlen(sbuf.mtext) + 1 ;
    
    

    /*
     * Send a message.
     */
    if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
		printf ("%i %l %s %d\n", msqid, sbuf.mtype, sbuf.mtext, buf_length);
        perror("msgsnd");
        exit(1);
    }

   else 
      printf("Message: \"%s\" Sent\n", sbuf.mtext);
      
    exit(0);
}
