#include <stdio.h>
#include <stdlib.h>
#include <ifaddrs.h> // for getting ip addresses
#include <netdb.h>
#include <inttypes.h>

#include <sys/types.h>
#include <linux/types.h>

#include <arpa/inet.h> // for getting local ip address

#include "logger.h"

#define LOOPBACKIP 16777343UL // Loopback IP address 127.0.0.1.

/** @brief Dumps section of memory to screen.
 *
 * Dumps a particular number of bytes to the log/screen.
 *
 * @param header [in] Text string that will be written before the data.
 * @param data [in] First byte of memory that will be dumped.
 * @param bytes [in] Number of bytes that will be dumped.
 *
 * @see http://moritzmolch.com/1136
 * @todo: Please check if double casting is OK?
 * 		(unsigned int)(intptr_t)data
 * @bug: bytes [in] should be pretty small so not to overrun the message[LOGSZ].
 */
void binary_dump(const char *header, char *data, unsigned int bytes) {
	unsigned int i = 0;
	char line[17] = {0};
	char temp[33] = {0};
	char message[LOGSZ] = {0};

	logger2(LOGGING_ALL, LOGGING_ALL, header);
	sprintf(message,"Binary Dump:\n");
    sprintf(temp, "%.8X | ", (unsigned int)(intptr_t)data);
    strcat(message,temp);
    while (i < bytes){
    	line[i%16] = *(data+i);

    	if((line[i%16] < 32) || (line[i%16] > 126)){
    		line[i%16] = '.';
    	}
    	sprintf(temp,"%.2X",(unsigned char)*(data+i));
    	strcat(message,temp);
    	i++;

    	if(i%4 == 0){

    		if (i%16 == 0){

    			if(i < bytes-1){
    				sprintf(temp," | %s\n%.8X | ", line, (unsigned int)(intptr_t)data+i);
    				strcat(message,temp);
    			}
    		}else{
    			sprintf(temp, " ");
    			strcat(message,temp);
    		}
    	}
    }
    while(i%16 > 0){

    	if(i%4 == 0){
    		sprintf(temp,"   ");
    		strcat(message,temp);
    	}else{
    		sprintf(temp,"  ");
    		strcat(message,temp);

    	}
    	i++;
    }
    sprintf(temp," | %s\n", line);
    strcat(message,temp);
    logger2(LOGGING_ALL, LOGGING_ALL, message);

    /*
	for(i=0; i < datalength && i < strlen(message); i++){
		buf_ptr += sprintf(buf_ptr, "%02X",data[i]);
	}
	logger2(LOGGING_WARN,DEBUG_IPC,message);
	*/
}

__u32 get_local_ip(){

char strIP[20];
char host[NI_MAXHOST];
struct ifaddrs *ifaddr, *ifa;
int s;
int i;
__u32 localID = 0;
__u32 tempIP = 0;
char message[LOGSZ];

if (getifaddrs(&ifaddr) == -1) {
        sprintf(message, "Initialization: Error opening interfaces.\n");
        logger(LOG_INFO, message);
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) { // loop through all interfaces.

        if (ifa->ifa_addr != NULL) {

            if (ifa->ifa_addr->sa_family == AF_INET) { // get all IPv4 addresses.
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

                if (s != 0) {
                    exit(EXIT_FAILURE);
                }

                inet_pton(AF_INET, (char *) &host, &tempIP); // convert string to decimal.

                /*
                 * Lets fine the largest local IP, and use that as localID
                 * Lets also exclude 127.0.0.1 as a valid ID.
                 */
                if ((tempIP > localID) && (tempIP != LOOPBACKIP)) {
                    localID = tempIP;
                }
            } // end get all IPv4 addresses.
        } // end ifa->ifa_addr NULL test.
    } // end loop through all interfaces.

    if (localID == 0) { // fail if no usable IP found.
        inet_ntop(AF_INET, &tempIP, strIP, INET_ADDRSTRLEN);
        sprintf(message, "Initialization: No usable IP Address. %s\n", strIP);
        logger(LOG_INFO, message);
        exit(EXIT_FAILURE);
    }

    return localID;

}
