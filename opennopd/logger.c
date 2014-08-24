#include <stdio.h>
#include <stdbool.h>
#include <syslog.h>
#include <netinet/in.h>
#include <linux/types.h>

#include "logger.h"
#include "opennopd.h"

/*
 * Logs a message to either the screen or to syslog.
 */

static int LOGGING_LEVEL	=	LOGGING_WARN;	// Default log everything up to WARN messages (regardless of individual component setting).

void logger(int LOG_TYPE, char *message) {
    if (isdaemon == true) {
        syslog(LOG_INFO, message);
    } else {
        printf(message);
    }
}

/** @brief Write a log message.
 *
 * We check to see what level of logging is enabled.
 * Then check if any particular logging was enabled.
 *
 * @param level [in] The log level for this message.
 * @param debug [in] The current debug level for the component sending the message.
 * @return int
 */
int logger2(int level, int debug, char *message) {

    if((level <= LOGGING_LEVEL) || (debug == LOGGING_ALL) || ((debug & level) == level)) {
        logger(LOG_INFO,message);
    }
    return 0;
}
