#ifndef DAEMON_H_
#define DAEMON_H_
#define _GNU_SOURCE

#include <linux/types.h>

/* Define states for worker threads. */
#define STOPPED -1
#define STOPPING 0
#define RUNNING 1

extern int servicestate;
extern __u32 localIP;
extern int isdaemon;

#endif
