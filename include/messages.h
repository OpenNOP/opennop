#ifndef MESSAGES_H_
#define MESSAGES_H_

#define MSGSZ     128

struct msgbuf {
	long mtype; // Type 1 = to the Daemon, all others are the PID the message is going to.
	pid_t sender; // Who sent the message.
	char mtext[MSGSZ]; // The commant entered or returning message.
};

void *messages_function(void *dummyPtr);

#endif /*MESSAGES_H_*/
