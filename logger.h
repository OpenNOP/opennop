#ifndef LOGGER_H_
#define LOGGER_H_
#define _GNU_SOURCE

#include <syslog.h>

void logger(int LOG_TYPE, char *message);
#endif
