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

/** @brief Check if we should log anything.
 * We can use this to determine if additional logging should be done.
 * Wrap binary dumps around this.
 *
 * @param messagelevel [in] The log level for this message.
 * @param componentlevel [in] The current debug level for the component sending the message.
 */
int should_i_log(int messagelevel, int componentlevel){
	if((messagelevel <= LOGGING_LEVEL) || (componentlevel == LOGGING_ALL) || ((componentlevel & messagelevel) == messagelevel)) {
	     return 1;
	}
	return 0;
}

/** @brief Write a log message.
 *
 * We check to see what level of logging is enabled.
 * Then check if any particular logging was enabled.
 *
 * @param messagelevel [in] The log level for this message.
 * @param componentlevel [in] The current debug level for the component sending the message.
 * @return int
 */
int logger2(int messagelevel, int componentlevel, char *message) {

    if(should_i_log(messagelevel, componentlevel) == 1) {
        logger(LOG_INFO,message);
    }
    return 0;
}
