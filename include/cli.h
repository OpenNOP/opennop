#ifndef CLI_H_
#define CLI_H_

#define MSGSZ     128

typedef struct msgbuf {
         long    mtype;
         char    mtext[MSGSZ];
 } message_buf;

void *cli_function (void *dummyPtr);

#endif /*CLI_H_*/
