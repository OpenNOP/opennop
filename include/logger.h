#ifndef LOGGER_H_
#define LOGGER_H_
#define _GNU_SOURCE

#include <syslog.h>

#define LOGSZ     4096

#define LOGGING_OFF		0	// Logging Level 0 00000000
#define LOGGING_FATAL	1	// Logging Level 1 00000001
#define LOGGING_ERROR	2	// Logging Level 2 00000010
#define LOGGING_WARN	4	// Logging Level 3 00000100
#define LOGGING_INFO	8	// Logging Level 4 00001000
#define LOGGING_DEBUG	16	// Logging Level 5 00010000
#define LOGGING_TRACE	32	// Logging Level 6 00100000
#define LOGGING_ALL		64	// Logging Level 7 01000000

void logger(int LOG_TYPE, char *message);
int logger2(int level, int debug, char *message);

#endif
