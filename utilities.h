/*
 * Converts char string to IP address.
 * This can be replaced with inet_ntop() or inet_pton() from <arpa/inet.h>.
 */
static inline __u32 
in_aton(char *str){
        __u32 l;
        __u16 val;
        __u8 i;

        l = 0;
        for (i = 0; i < 4; i++) {
                l <<= 8;
                if (*str != '\0') {
                        val = 0;
                        while (*str != '\0' && *str != '.') {
                                val *= 10;
                                val += *str - '0';
                                str++;
                        }
                        l |= val;
                        if (*str != '\0')
                                str++;
                }
        }
        return(htonl(l));
}

/*
 * Puts sockets in order.
 */
void sort_sockets(__u32 *largerIP, __u16 *largerIPPort, __u32 *smallerIP, __u16 *smallerIPPort,
					__u32 saddr, __u16 source, __u32 daddr, __u16 dest)
{
	if (saddr > daddr){ // Using source IP address as largerIP.
		*largerIP = saddr;
		*largerIPPort = source;
		*smallerIP = daddr;
		*smallerIPPort = dest;
	}
	else { // Using destination IP address as largerIP.
		*largerIP = daddr;
		*largerIPPort = dest;
		*smallerIP = saddr;
		*smallerIPPort = source;
	}
}

/*
 * Logs a message to either the screen or to syslog.
 */
void logger(char *message)
{
	if (isdaemon == TRUE){
		syslog(LOG_INFO, message);
	}
	else{
		printf(message);
	}	
}
