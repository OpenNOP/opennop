#ifndef LOGGER_H_
#define LOGGER_H_
#define _GNU_SOURCE

#include <syslog.h>

#define LOGSZ     256

void logger(int LOG_TYPE, char *message);
#endif
