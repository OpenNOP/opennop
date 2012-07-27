#ifndef CLI_H_
#define CLI_H_

#define MSGSZ     128

typedef struct msgbuf
{
    long	mtype; // Type 1 = to the Daemon, all others are the PID the message is going to.
    pid_t	sender; // Who sent the message.
    char	mtext[MSGSZ]; // The commant entered or returning message.
}
message_buf;

void *cli_function (void *dummyPtr);

#endif /*CLI_H_*/
