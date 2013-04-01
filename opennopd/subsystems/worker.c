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
#include "memorymanager.h"

struct worker workers[MAXWORKERS]; // setup slots for the max number of workers.
unsigned char numworkers = 0; // sets number of worker threads. 0 = auto detect.
int DEBUG_WORKER = false;

void *optimization_function (void *dummyPtr)
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
    me = dummyPtr;

    me->optimization.lzbuffer = calloc(1,BUFSIZE + 400);
	/* Sharwan J: QuickLZ buffer needs (original data size + 400 bytes) buffer */
	if (me->optimization.lzbuffer == NULL){
		sprintf(message, "Worker: Couldn't allocate buffer");
		logger(LOG_INFO, message);
		exit(1);
	}

    if (me->optimization.lzbuffer != NULL)
    {

        while (me->state >= STOPPING)
        {

            thispacket = dequeue_packet(&me->optimization.queue, true);

            if (thispacket != NULL)
            { // If a packet was taken from the queue.
                iph = (struct iphdr *)thispacket->data;
                tcph = (struct tcphdr *) (((u_int32_t *)iph) + iph->ihl);

                if (DEBUG_WORKER == true)
                {
                    sprintf(message, "Worker: IP Packet length is: %u\n",ntohs(iph->tot_len));
                    logger(LOG_INFO, message);
                }
                me->optimization.metrics.bytesin += ntohs(iph->tot_len);
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
	                            tcp_compress((__u8 *)iph, me->optimization.lzbuffer,state_compress);
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
                        me->optimization.metrics.bytesout += ntohs(iph->tot_len);
                        nfq_set_verdict(thispacket->hq, thispacket->id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)thispacket->data);
                        put_freepacket_buffer(thispacket);
                        thispacket = NULL;
                    }

                } /* End NULL session check. */
                else
                { /* Session was NULL. */
                	me->optimization.metrics.bytesout += ntohs(iph->tot_len);
                    nfq_set_verdict(thispacket->hq, thispacket->id, NF_ACCEPT, 0, NULL);
                    put_freepacket_buffer(thispacket);
                    thispacket = NULL;
                }
                me->optimization.metrics.packets++;
            } /* End NULL packet check. */
        } /* End working loop. */
        free(me->optimization.lzbuffer);
		free(state_compress);
        me->optimization.lzbuffer = NULL;
    }
    return NULL;
}

void *deoptimization_function (void *dummyPtr)
{
    struct worker *me = NULL;
    struct packet *thispacket = NULL;
    struct session *thissession = NULL;
    struct iphdr *iph = NULL;
    struct tcphdr *tcph = NULL;
    __u32 largerIP, smallerIP, acceleratorID;
    __u16 largerIPPort, smallerIPPort;
    char message [LOGSZ];
	qlz_state_decompress *state_decompress = (qlz_state_decompress *)malloc(sizeof(qlz_state_decompress));
    me = dummyPtr;

    me->deoptimization.lzbuffer = calloc(1,BUFSIZE + 400);
	/* Sharwan J: QuickLZ buffer needs (original data size + 400 bytes) buffer */
	if (me->deoptimization.lzbuffer == NULL){
		sprintf(message, "Worker: Couldn't allocate buffer");
		logger(LOG_INFO, message);
		exit(1);
	}

    if (me->deoptimization.lzbuffer != NULL){

        while (me->state >= STOPPING){

            thispacket = dequeue_packet(&me->deoptimization.queue, true);

            if (thispacket != NULL)
            { // If a packet was taken from the queue.
                iph = (struct iphdr *)thispacket->data;
                tcph = (struct tcphdr *) (((u_int32_t *)iph) + iph->ihl);

                if (DEBUG_WORKER == true)
                {
                    sprintf(message, "Worker: IP Packet length is: %u\n",ntohs(iph->tot_len));
                    logger(LOG_INFO, message);
                }
                me->deoptimization.metrics.bytesin += ntohs(iph->tot_len);
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

                        if (acceleratorID != 0){

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
                                    if (tcp_decompress((__u8 *)iph, me->deoptimization.lzbuffer, state_decompress) == 0)
                                    { // Decompression failed if 0.
                                        nfq_set_verdict(thispacket->hq, thispacket->id, NF_DROP, 0, NULL); // Decompression failed drop.
                                        put_freepacket_buffer(thispacket);
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
                        me->deoptimization.metrics.bytesout += ntohs(iph->tot_len);
                        nfq_set_verdict(thispacket->hq, thispacket->id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)thispacket->data);
                        put_freepacket_buffer(thispacket);
                        thispacket = NULL;
                    }

                } /* End NULL session check. */
                else
                { /* Session was NULL. */
                	me->deoptimization.metrics.bytesout += ntohs(iph->tot_len);
                    nfq_set_verdict(thispacket->hq, thispacket->id, NF_ACCEPT, 0, NULL);
                    put_freepacket_buffer(thispacket);
                    thispacket = NULL;
                }
                me->deoptimization.metrics.packets++;
            } /* End NULL packet check. */
        } /* End working loop. */
        free(me->deoptimization.lzbuffer);
		free(state_decompress);
		me->deoptimization.lzbuffer = NULL;
    }
    return NULL;
}

/*
 * Returns how many workers should be running.
 */
unsigned char get_workers(void){
	return numworkers;
}

/*
 * Sets how many workers should be running.
 */
void set_workers(unsigned char desirednumworkers){
	numworkers = desirednumworkers;
}
