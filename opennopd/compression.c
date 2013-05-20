#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options
#include "compression.h"
#include "quicklz.h"
#include "tcpoptions.h"
#include "logger.h"
#include "climanager.h"

int compression = true; // Determines if opennop should compress tcp data.
int DEBUG_COMPRESSION = false;

int cli_show_compression(int client_fd) {
	char msg[MAX_BUFFER_SIZE] = { 0 };
	if (compression == true) {
		sprintf(msg, "compression enabled\n");
	} else {
		sprintf(msg, "compression disabled\n");
	}
	cli_send_feedback(client_fd, msg);
	return 0;
}

int cli_compression_enable(int client_fd) {
	compression = true;
	char msg[MAX_BUFFER_SIZE] = { 0 };
	sprintf(msg, "compression enabled\n");
	cli_send_feedback(client_fd, msg);
	return 0;
}

int cli_compression_disable(int client_fd) {
	compression = false;
	char msg[MAX_BUFFER_SIZE] = { 0 };
	sprintf(msg, "compression disabled\n");
	cli_send_feedback(client_fd, msg);
	return 0;
}

/*
 * Compresses the TCP data of an SKB.
 */
unsigned int tcp_compress(__u8 *ippacket, __u8 *lzbuffer,
		qlz_state_compress *state_compress) {
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	__u16 oldsize = 0, newsize = 0; /* Store old, and new size of the TCP data. */
	__u8 *tcpdata = NULL; /* Starting location for the TCP data. */
	char message[LOGSZ];

	if (DEBUG_COMPRESSION == true) {
		sprintf(message, "[OpenNOP]: Entering into TCP COMPRESS \n");
		logger(LOG_INFO, message);
	}

	// If the skb or state_compress is NULL abort compression.
	if ((ippacket != NULL) && (NULL != state_compress) && (compression == true)) {
		iph = (struct iphdr *) ippacket; // Access ip header.
		memset(state_compress, 0, sizeof(qlz_state_compress));

		if ((iph->protocol == IPPROTO_TCP)) { // If this is not a TCP segment abort compression.
			tcph = (struct tcphdr *) (((u_int32_t *) ippacket) + iph->ihl);
			oldsize = (__u16)(ntohs(iph->tot_len) - iph->ihl * 4) - tcph->doff
					* 4;
			tcpdata = (__u8 *) tcph + tcph->doff * 4; // Find starting location of the TCP data.

			if (DEBUG_COMPRESSION == true) {
				sprintf(message,
						"Compression: Original TCP data length is: %u\n",
						oldsize);
				logger(LOG_INFO, message);
			}

			if (oldsize > 0) { // Only compress if there is any data.
				newsize = (oldsize * 2);

				if (lzbuffer != NULL) {

					if (DEBUG_COMPRESSION == true) {
						sprintf(message, "Compression: Begin compression.\n");
						logger(LOG_INFO, message);
					}

					newsize = qlz_compress((char *) tcpdata, (char *) lzbuffer,
							oldsize, state_compress);
				} else {

					if (DEBUG_COMPRESSION == true) {
						sprintf(message, "Compression: lzbuffer was NULL!\n");
						logger(LOG_INFO, message);
					}
				}

				if (DEBUG_COMPRESSION == true) {
					sprintf(message,
							"Compression: New TCP data length is: %u\n",
							newsize);
					logger(LOG_INFO, message);
				}

				if (newsize < oldsize) {
					memmove(tcpdata, lzbuffer, newsize); // Move compressed data to packet.
					//pskb_trim(skb,skb->len - (oldsize - newsize)); // Remove extra space from skb.
					iph->tot_len = htons(ntohs(iph->tot_len) - (oldsize
							- newsize));// Fix packet length.
					__set_tcp_option((__u8 *) iph, 31, 3, 1); // Set compression flag.
					tcph->seq = htonl(ntohl(tcph->seq) + 8000); // Increase SEQ number.

					if (DEBUG_COMPRESSION == true) {
						sprintf(message,
								"Compressing [%d] size of data to [%d] \n",
								oldsize, newsize);
						logger(LOG_INFO, message);
					}
				}

				if (DEBUG_COMPRESSION == true) {
					sprintf(message, "[OpenNOP]: Leaving TCP COMPRESS \n");
					logger(LOG_INFO, message);
				}
			}
		}
	}
	return 1;
}

/*
 * Decompress the TCP data of an SKB.
 */
unsigned int tcp_decompress(__u8 *ippacket, __u8 *lzbuffer,
		qlz_state_decompress *state_decompress) {
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	__u16 oldsize = 0, newsize = 0; /* Store old, and new size of the TCP data. */
	__u8 *tcpdata = NULL; /* Starting location for the TCP data. */
	char message[LOGSZ];

	if (DEBUG_COMPRESSION == true) {
		sprintf(message, "[OpenNOP]: Entering into TCP DECOMPRESS \n");
		logger(LOG_INFO, message);
	}

	if ((ippacket != NULL) && (NULL != state_decompress)) { // If the skb or state_decompress is NULL abort compression.
		iph = (struct iphdr *) ippacket; // Access ip header.
		memset(state_decompress, 0, sizeof(qlz_state_decompress));

		if ((iph->protocol == IPPROTO_TCP)) { // If this is not a TCP segment abort compression.
			tcph = (struct tcphdr *) (((u_int32_t *) ippacket) + iph->ihl); // Access tcp header.
			oldsize = (__u16)(ntohs(iph->tot_len) - iph->ihl * 4) - tcph->doff
					* 4;

			tcpdata = (__u8 *) tcph + tcph->doff * 4; // Find starting location of the TCP data.

			if ((oldsize > 0) && (lzbuffer != NULL)) {

				newsize = qlz_decompress((char *) tcpdata, (char *) lzbuffer,
						state_decompress);
				memmove(tcpdata, lzbuffer, newsize); // Move decompressed data to packet.
				iph->tot_len = htons(ntohs(iph->tot_len) + (newsize - oldsize));// Fix packet length.
				__set_tcp_option((__u8 *) iph, 31, 3, 0); // Set compression flag to 0.
				tcph->seq = htonl(ntohl(tcph->seq) - 8000); // Decrease SEQ number.

				if (DEBUG_COMPRESSION == true) {
					sprintf(
							message,
							"[OpenNOP] Decompressing [%d] size of data to [%d] \n",
							oldsize, newsize);
					logger(LOG_INFO, message);
				}
				return 1;
			}
		}
	}
	return 0;
}
