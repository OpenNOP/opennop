#include <stdio.h>
#include <stdbool.h>
#include <syslog.h>
#include <netinet/in.h>
#include <linux/types.h>

#include "../include/logger.h"
#include "../include/opennopd.h"

/*
 * Logs a message to either the screen or to syslog.
 */
void logger(int LOG_TYPE, char *message)
{
	if (isdaemon == true){
		syslog(LOG_INFO, message);
	}
	else{
		printf(message);
	}	
}
