#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h> // for multi-threading

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include <linux/types.h>
#include <linux/netfilter.h> // for NF_ACCEPT

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue

#include "queuemanager.h"
#include "worker.h"
#include "opennopd.h"
#include "packet.h"
#include "compression.h"
#include "csum.h"
#include "sessionmanager.h"
#include "tcpoptions.h"
#include "logger.h"
#include "quicklz.h"

struct worker workers[MAXWORKERS]; // setup slots for the max number of workers.
unsigned char numworkers = 0; // sets number of worker threads. 0 = auto detect.
int DEBUG_WORKER = false;

void *worker_function (void *dummyPtr)
{
    struct worker *me = NULL;
    struct packet *thispacket = NULL;
    struct session *thissession = NULL;
    struct iphdr *iph = NULL;
    struct tcphdr *tcph = NULL;
    __u32 largerIP, smallerIP, acceleratorID;
    __u16 largerIPPort, smallerIPPort;
    char message [LOGSZ];
	qlz_state_compress *state_compress = (qlz_state_compress *)malloc(sizeof(qlz_state_compress));
	qlz_state_decompress *state_decompress = (qlz_state_decompress *)malloc(sizeof(qlz_state_decompress));
    me = dummyPtr;

    me->lzbuffer = calloc(1,sizeof(BUFSIZE) + 400);
	/* Sharwan J: QuickLZ buffer needs (original data size + 400 bytes) buffer */
	if ( NULL == me->lzbuffer){
		sprintf(message, "Worker: Couldn't allocate buffer");
		exit(1);
	}

    if (me->lzbuffer != NULL) 
    {

        while (me->state >= STOPPING)
        {

            thispacket = dequeue_packet(&me->queue, true);

            if (thispacket != NULL)
            { // If a packet was taken from the queue.
                iph = (struct iphdr *)thispacket->data;
                tcph = (struct tcphdr *) (((u_int32_t *)iph) + iph->ihl);

                if (DEBUG_WORKER == true)
                {
                    sprintf(message, "Worker: IP Packet length is: %u\n",ntohs(iph->tot_len));
                    logger(LOG_INFO, message);
                }

                acceleratorID = (__u32)__get_tcp_option((__u8 *)iph,30);

                /* Check what IP address is larger. */
                sort_sockets(&largerIP, &largerIPPort, &smallerIP, &smallerIPPort,
                             iph->saddr,tcph->source,iph->daddr,tcph->dest);

                if (DEBUG_WORKER == true)
                {
                    sprintf(message, "Worker: Searching for session.\n");
                    logger(LOG_INFO, message);
                }

                thissession = getsession(largerIP, largerIPPort, smallerIP, smallerIPPort);

                if (thissession != NULL)
                {

                    if (DEBUG_WORKER == true)
                    {
                        sprintf(message, "Worker: Found a session.\n");
                        logger(LOG_INFO, message);
                    }

                    if ((tcph->syn == 0) && (tcph->ack == 1) && (tcph->fin == 0))
                    {

                        if (acceleratorID == 0)
                        { // Accelerator ID was not found.

                            if (iph->saddr == largerIP)
                            { // Set the Accelerator for this source.
                                thissession->largerIPAccelerator = localIP;
                            }
                            else
                            {
                                thissession->smallerIPAccelerator = localIP;
                            }

                            __set_tcp_option((__u8 *)iph,30,6,localIP); // Add the Accelerator ID to this packet.

                            if ((((iph->saddr == largerIP) &&
                                    (thissession->largerIPAccelerator == localIP) &&
                                    (thissession->smallerIPAccelerator != 0) &&
                                    (thissession->smallerIPAccelerator != localIP)) ||
                                    ((iph->saddr == smallerIP) &&
                                     (thissession->smallerIPAccelerator == localIP) &&
                                     (thissession->largerIPAccelerator != 0) &&
                                     (thissession->largerIPAccelerator != localIP))) &&
                                    (thissession->state == TCP_ESTABLISHED))
                            {

                                /*
                                	 * Do some acceleration!
                                	 */

                                if (DEBUG_WORKER == true)
                                {
                                    sprintf(message, "Worker: Compressing packet.\n");
                                    logger(LOG_INFO, message);
                                }
	                            tcp_compress((__u8 *)iph, me->lzbuffer,state_compress);
                            }
                            else
                            {

                                if (DEBUG_WORKER == true)
                                {
                                    sprintf(message, "Worker: Not compressing packet.\n");
                                    logger(LOG_INFO, message);
                                }
                            }
                        }
                        else
                        {

                            if (iph->saddr == largerIP)
                            { // Set the Accelerator for this source.
                                thissession->largerIPAccelerator = acceleratorID;
                            }
                            else
                            {
                                thissession->smallerIPAccelerator = acceleratorID;
                            }

                            if (__get_tcp_option((__u8 *)iph,31) != 0)
                            { // Packet is flagged as compressed.

                                if (DEBUG_WORKER == true)
                                {
                                    sprintf(message, "Worker: Packet is compressed.\n");
                                    logger(LOG_INFO, message);
                                }

                                if (((iph->saddr == largerIP) &&
                                        (thissession->smallerIPAccelerator == localIP)) ||
                                        ((iph->saddr == smallerIP) &&
                                         (thissession->largerIPAccelerator == localIP)))
                                {

                                    /*
                                     * Decompress this packet!
                                     */
                                    if (tcp_decompress((__u8 *)iph, me->lzbuffer, state_decompress) == 0)
                                    { // Decompression failed if 0.
                                        nfq_set_verdict(thispacket->hq, thispacket->id, NF_DROP, 0, NULL); // Decompression failed drop.
                                        free(thispacket);
                                        thispacket = NULL;
                                    }
                                }
                            }
                        }
                    }

                    if (tcph->rst == 1)
                    { // Session was reset.

                        if (DEBUG_WORKER == true)
                        {
                            sprintf(message, "Worker: Session was reset.\n");
                            logger(LOG_INFO, message);
                        }
                        clearsession(thissession);
                        thissession = NULL;
                    }

                    /* Normal session closing sequence. */
                    if (tcph->fin == 1)
                    { // Session is being closed.

                        if (DEBUG_WORKER == true)
                        {
                            sprintf(message, "Worker: Session is closing.\n");
                            logger(LOG_INFO, message);
                        }

                        switch (thissession->state)
                        {
                        case TCP_ESTABLISHED:
                            thissession->state = TCP_CLOSING;
                            break;

                        case TCP_CLOSING:
                            clearsession(thissession);
                            thissession = NULL;
                            break;
                        }
                    }

                    if (thispacket != NULL)
                    {
                        /*
                        	 * Changing anything requires the IP and TCP
                        	 * checksum to need recalculated.
                         	 */
                        checksum(thispacket->data);
                        nfq_set_verdict(thispacket->hq, thispacket->id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)thispacket->data);
                        free(thispacket);
                        thispacket = NULL;
                    }

                } /* End NULL session check. */
                else
                { /* Session was NULL. */
                    nfq_set_verdict(thispacket->hq, thispacket->id, NF_ACCEPT, 0, NULL);
                    free(thispacket);
                    thispacket = NULL;
                }
            } /* End NULL packet check. */
        } /* End working loop. */
        free(me->lzbuffer);
		free(state_compress);
		free(state_decompress);
        me->lzbuffer = NULL;
    }
    return NULL;
}
