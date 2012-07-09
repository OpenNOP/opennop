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




void *cli_function (void *dummyPtr){
	int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    message_buf rbuf;
    char message [LOGSZ];
    
	/*
	 * Get the message queue id for the
	 * "name" 1234, which was created by
	 * the server.
	 */
	if ((key = ftok("/var/opennop",'o')) == -1) { /* Create the opennop key */
		sprintf(message, "Could not create the opennop message queue key.");
        logger(LOG_INFO, message);
        sprintf(message, "Does /var/opennop exist?");
        logger(LOG_INFO, message);
        exit(1);
	}
    
    sprintf(message, "\nmsgget: Calling msgget(%#x, %#o)\n", key, msgflg);
	logger(LOG_INFO, message);
	
	if ((msqid = msgget(key, msgflg )) == -1) { /* Connect to the message queue */
        sprintf(message, "Could not connect to the message queue.");
        logger(LOG_INFO, message);
        exit(1);
    }else{
		sprintf(message,"msgget: msgget succeeded: msqid = %d\n", msqid);
		logger(LOG_INFO, message);
	}
	
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
		if (msgrcv(msqid, &rbuf, MSGSZ, 1, 0) < 0) {
			perror("msgrcv");
			exit(1);
		}

		/*
		 * Print the answer.
		 */
		sprintf(message,"%s\n", rbuf.mtext);
		logger(LOG_INFO, message);
		
		
		/*
		 * This seems to be for sending a message.
		 * I'll need to use this to reply back to the user interface.
		 */
		
		//if (msgrcv(msqid, &sbuf, sizeof(sbuf.mtext), 0, 0) == -1) {
       	// 	sprintf(message, "Error receiving message.");
        //	logger(LOG_INFO, message);
        //	exit(1);
		//}else{
		//	sprintf(message, sbuf.mtext);
        //	logger(LOG_INFO, message);
		//}			
		
		/*
		 * 
		 * This is for getting a message from the queue.
		 
		size_t buf_length;
		sbuf.mtype = 1; //TWhen we use this it will be the PID of the CLI.
		//fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);
		strcpy(sbuf.mtext, "Did you get this?");
    	//fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);
		buf_length = strlen(sbuf.mtext) + 1 ;
		
		if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0){
			//sprintf(message, "%i, %l, %s, %d\n", msqid, sbuf.mtype, sbuf.mtext, buf_length);
			sprintf(message, "Error sending message.");
			logger(LOG_INFO, message);
    	}else{
      		sprintf(message, "Message: \"%s\" Sent\n", sbuf.mtext);
      		logger(LOG_INFO, message);
		}
		*/
		
		
		
	}
	return NULL;
}
