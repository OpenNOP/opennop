#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for sleep function

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <linux/types.h>

#include "daemon.h"
#include "logger.h"

#define MSGSZ     128

typedef struct msgbuf {
         long    mtype;
         char    mtext[MSGSZ];
 } message_buf;


void *cli_function (void *dummyPtr){
	int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    message_buf sbuf;
    size_t buf_length;
    char message [LOGSZ];
    
	/*
	 * Get the message queue id for the
	 * "name" 1234, which was created by
	 * the server.
	 */
	key = ftok("/var/opennop",'o');
    
    sprintf(message, "\nmsgget: Calling msgget(%#x, %#o)\n", key, msgflg);
	logger(LOG_INFO, message);
	
	if ((msqid = msgget(key, msgflg )) < 0) {
        sprintf(message, "msgget");
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
		
		sbuf.mtype = 1;
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
		sleep(15); // Sleeping for 15 seconds.
	}
	return NULL;
}
