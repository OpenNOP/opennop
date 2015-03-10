#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // for sleep function

#include <sys/time.h> 

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include <linux/types.h>

#include "sessioncleanup.h"
#include "sessionmanager.h"
#include "opennopd.h"
#include "packet.h"
#include "tcpoptions.h"
#include "csum.h"
#include "logger.h"
#include "climanager.h"

static int rawsock = 0; // Used to send keep-alive messages.
static int dead_session_detection = true; //Detect dead sessions by default.
static int cleanup_timer = 30; // Time in seconds the dead session detection should run.

static int DEBUG_SESSION_TRACKING = LOGGING_INFO;

static pthread_t t_cleanup; // thread for cleaning up dead sessions.

/*
 * This sends a keep alive message to the specified IP.
 * It does not tag the packet with the Accelerator ID this
 * prevents the keepalive messages from recreating dead sessions
 * in remote Accelerators.
 */
void sendkeepalive
(__u32 saddr, __u16 source, __u32 seq,
__u32 daddr, __u16 dest, __u32 ack_seq){
	char packet[BUFSIZE];
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	struct sockaddr_in sin, din;
	__u16 tcplen;
	char message[LOGSZ];

    sprintf(message, "Keepalive: seq->%u.\n",seq);
    logger2(LOGGING_DEBUG,LOGGING_DEBUG,message);

    sprintf(message, "Keepalive: ack->%u.\n",ack_seq);
    logger2(LOGGING_DEBUG,LOGGING_DEBUG,message);

	memset(packet, 0, BUFSIZE);

	sin.sin_family = AF_INET;
	din.sin_family = AF_INET;
	
	sin.sin_port = source;
	din.sin_port = dest;
	
	sin.sin_addr.s_addr = saddr;
	din.sin_addr.s_addr = daddr;

	iph = (struct iphdr *)packet;
	iph->ihl = 5; // IP header length.
	iph->version = 4; // IP version 4.
	iph->tos = 0; // No TOS.
	iph->tot_len=htons(sizeof(struct iphdr) + sizeof(struct tcphdr)); // L3 + L4 header length.
	iph->id = 0; // What?
	iph->frag_off = 0; // No fragmenting.
	iph->ttl = 64; // Set a TTL.
	iph->protocol = IPPROTO_TCP; // TCP protocol.
	iph->check = 0; // No IP checksum yet.
	iph->saddr = saddr; // Source IP.
	iph->daddr = daddr; // Dest IP.
	
	tcph = (struct tcphdr *) (((u_int32_t *)iph) + iph->ihl);
	tcph->check = 0; // No TCP checksum yet.
	tcph->source = source; // Source TCP Port.
	tcph->dest = dest; // Destination TCP Port.
	tcph->seq = htonl(seq - 1); // Current SEQ minus one is used for TCP keepalives.
	tcph->ack_seq = htonl(ack_seq); // Ummm not sure yet.
	tcph->res1 = 0; // Not sure.
	tcph->doff = 5; // TCP Offset.  At least 5 if there are no TCP options.
	tcph->fin = 0; // FIN flag.
	tcph->syn = 0; // SYN flag.
	tcph->rst = 0; // RST flag.
	tcph->psh = 0; // PSH flag.
	tcph->ack = 1; // ACK flag.
	tcph->urg = 0; // URG flag.
	tcph->window = htons(65535);
	
	//__set_tcp_option((__u8 *)iph,30,6,localID); // Add the Accelerator ID to this packet.
	set_nod_header_data((__u8 *)iph, ONOP, get_opennop_id(), OPENNOP_IPC_ID_LENGTH);
	
	tcplen = ntohs(iph->tot_len) - iph->ihl*4;
	tcph->check = 0;
	tcph->check = tcp_sum_calc(tcplen,
		(unsigned short *)&iph->saddr,
		(unsigned short *)&iph->daddr,
		(unsigned short *)tcph);
 	iph->check = 0;
 	iph->check = ip_sum_calc(iph->ihl*4,
		(unsigned short *)iph);
	
	
	if(sendto(rawsock, packet, ntohs(iph->tot_len), 0, (struct sockaddr *)&din, sizeof(din)) < 0){
	    sprintf(message, "Failed sending keepalive.\n");
	    logger2(LOGGING_INFO,DEBUG_SESSION_TRACKING,message);
	}
	
	return;
}

/*
 * This function goes through All the session of a single list
 * looking for idle sessions.  When it locates one it
 * send a keepalive message to both client/server.  If they
 * fail to respond to the keepalive messages twice the session
 * is removed from its list. 
 */
void cleanuplist (struct session_head *currentlist){
	struct session *currentsession = NULL;
	char message[LOGSZ];
	
	if (currentlist->next != NULL){ // Make sure there is something in the list.
		currentsession = currentlist->next; // Make the first session of the list the current.
	}
	else{ // No sessions in this list.
		return; 
	}
	
	if (currentsession != NULL){ // Make sure there is a session to work on.
	
		while (currentsession != NULL){ // Do this for all the sessions in the list.
			
			if (currentsession->deadcounter > 2){ // This session has failed to respond it is dead.
				
				if (currentsession->next != NULL){ // Check that next sessions isnt NULL;
					currentsession = currentsession->next; // Advance to the next session.
					clearsession(currentsession->prev); // Clear the previous session.
				}
				else{ // This was the last session of the list.
					clearsession(currentsession);
					currentsession = NULL;
				}
			    sprintf(message, "Dead session removed.\n");
			    logger2(LOGGING_INFO,DEBUG_SESSION_TRACKING,message);
			}
			else{ // Session needs checked for idle time.
				
				/*
				if (currentsession->lastactive < currenttime){ // If this session is not active.
					currentsession->deadcounter++; // Increment the deadcounter.
					sendkeepalive(currentsession->larger->address, currentsession->larger->port, currentsession->larger->sequence,
									currentsession->smaller->address, currentsession->smaller->port, currentsession->smaller->sequence);
									
					sendkeepalive(currentsession->smaller->address, currentsession->smaller->port, currentsession->smaller->sequence,
									currentsession->larger->address, currentsession->larger->port, currentsession->larger->sequence);
					 
				}
				*/


/*
				if(currentsession->larger->sequence == currentsession->largerIPPreviousseq){
					sendkeepalive(currentsession->smaller->address, currentsession->smaller->port, currentsession->smaller->sequence,
									currentsession->larger->address, currentsession->larger->port, currentsession->larger->sequence);

					currentsession->deadcounter++;

				}else if(currentsession->smaller->sequence == currentsession->smallerIPPreviousseq){
					sendkeepalive(currentsession->larger->address, currentsession->larger->port, currentsession->larger->sequence,
									currentsession->smaller->address, currentsession->smaller->port, currentsession->smaller->sequence);

					currentsession->deadcounter++;
				}else{
					currentsession->deadcounter = 0;
				}

				currentsession->largerIPPreviousseq = currentsession->larger->sequence;
				currentsession->smallerIPPreviousseq = currentsession->smaller->sequence;
*/

				if (currentsession->next != NULL){ // Check if there are more sessions.
					currentsession = currentsession->next; // Advance to the next session.
				}
				else{ // Reached end of the session list.
					currentsession = NULL;
				}
			}	
		}
	}
}

/*
 * This function runs every 5 or so minuets.
 * It checks each bucket for idle sessions, and
 * will remove the dead ones using the previous function.
 */
void *cleanup_function(void *data){
	int one = 1;
	const int *val = &one;
	char message [LOGSZ];
	__u32 i = 0;
	
	rawsock = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
	
	if (rawsock < 0){
		sprintf(message, "Initialization: Error opening raw socket.\n");
		logger(LOG_INFO, message);
		exit(EXIT_FAILURE);
	}
	
	if(setsockopt(rawsock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0){
		sprintf(message, "Initialization: Error setting socket options.\n");
		logger(LOG_INFO, message);
		exit(EXIT_FAILURE);
	}
	
	while (servicestate >= STOPPING) {
		sleep(cleanup_timer); // Sleeping for 5 minuets.

		if(dead_session_detection == true){
		
			for (i = 0; i < SESSIONBUCKETS; i++){  // Process each bucket.
		
				if (getsessionhead(i)->next != NULL){
    				cleanuplist(getsessionhead(i));
    			}
			}
		}else{
		    sprintf(message, "Skipping dead session detection.\n");
		    logger2(LOGGING_INFO,DEBUG_SESSION_TRACKING,message);
		}

	}
	
	/*
	 * Free any allocated resources before exiting
	 */
	close(rawsock); // close the socket used for sending keepalives.
	rawsock = 0;
        
	return NULL;
}

struct commandresult cli_show_dead_session_detection(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result  = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };

	if (dead_session_detection == true) {
		sprintf(msg, "dead session detection enabled\n");
	} else {
		sprintf(msg, "dead session detection disabled\n");
	}
	cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

struct commandresult cli_dead_session_detection_enable(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };
	dead_session_detection = true;
	sprintf(msg, "dead session detection enabled\n");
	cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

struct commandresult cli_dead_session_detection_disable(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };
	dead_session_detection = false;
	sprintf(msg, "dead session detection disabled\n");
	cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

void start_dead_session_detection() {

    register_command(NULL, "show dead session detection", cli_show_dead_session_detection, false, false);
    register_command(NULL, "dead session detection enable", cli_dead_session_detection_enable, false, false);
    register_command(NULL, "dead session detection disable", cli_dead_session_detection_disable, false, false);

    pthread_create(&t_cleanup, NULL, cleanup_function, (void *) NULL);

}

void rejoin_dead_session_detection() {
    pthread_join(t_cleanup, NULL);
}
