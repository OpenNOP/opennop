#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <sys/time.h>

#include <arpa/inet.h> // for getting local ip address
#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options
#include <linux/netfilter.h> // for NF_ACCEPT
#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue
#include "fetcher.h"
#include "queuemanager.h"
#include "sessionmanager.h"
#include "logger.h"
#include "tcpoptions.h"
#include "csum.h"
#include "packet.h"
#include "opennopd.h"
#include "worker.h"
#include "memorymanager.h"
#include "counters.h"
#include "climanager.h"
#include "ipc.h"

struct fetcher thefetcher;

int DEBUG_FETCHER = false;
int DEBUG_FETCHER_COUNTERS = false;
int G_SCALEWINDOW = 7;

struct nfq_handle *h;
struct nfq_q_handle *qh;
int fd;

int fetcher_callback(struct nfq_q_handle *hq, struct nfgenmsg *nfmsg,
                     struct nfq_data *nfa, void *data) {
    u_int32_t id = 0;
    struct iphdr *iph = NULL;
    struct tcphdr *tcph = NULL;
    struct session *thissession = NULL;
    struct packet *thispacket = NULL;
    struct nfqnl_msg_packet_hdr *ph;
    __u32 largerIP, smallerIP;
    __u16 largerIPPort, smallerIPPort, mms;
    int ret;
    unsigned char *originalpacket = NULL;
    char *remoteID = NULL;
    char message[LOGSZ];
    char strIP[20];
    //struct packet *newpacket = NULL;

    ph = nfq_get_msg_packet_hdr(nfa);

    if (ph) {
        id = ntohl(ph->packet_id);
    }

    ret = nfq_get_payload(nfa, &originalpacket);

    if (servicestate >= RUNNING) {
        iph = (struct iphdr *) originalpacket;

        thefetcher.metrics.bytesin += ntohs(iph->tot_len);

        /* We need to double check that only TCP packets get accelerated. */
        /* This is because we are working from the Netfilter QUEUE. */
        /* User could QUEUE UDP traffic, and we cannot accelerate UDP. */
        if ((iph->protocol == IPPROTO_TCP) && (id != 0)) {

            tcph = (struct tcphdr *)(((u_int32_t *) originalpacket) + iph->ihl);

            /* Check what IP address is larger. */
            sort_sockets(&largerIP, &largerIPPort, &smallerIP, &smallerIPPort, iph->saddr, tcph->source, iph->daddr, tcph->dest);

            //remoteID = (__u32) __get_tcp_option((__u8 *)originalpacket,30);
            remoteID = get_nod_header_data((__u8 *)iph, ONOP).data;

            if (DEBUG_FETCHER == true) {
                inet_ntop(AF_INET, remoteID, strIP, INET_ADDRSTRLEN);
                sprintf(message, "Fetcher: The accelerator ID is:%s.\n", strIP);
                logger(LOG_INFO, message);
            }

            /* Check if this a SYN packet to identify a new session. */
            /* This packet will not be placed in a work queue, but  */
            /* will be accepted here because it does not have any data. */
            if ((tcph->syn == 1) && (tcph->ack == 0)) {
                thissession = getsession(largerIP, largerIPPort, smallerIP, smallerIPPort); // Check for an outstanding syn.

                if (thissession == NULL) {
                    thissession = insertsession(largerIP, largerIPPort, smallerIP, smallerIPPort); // Insert into sessions list.
                }

                /* We need to check for NULL to make sure */
                /* that a record for the session was created */
                if (thissession != NULL) {

                    if (DEBUG_FETCHER == true) {
                        sprintf(message, "Fetcher: The session manager created a new session.\n");
                        logger(LOG_INFO, message);
                    }

                    sourceisclient(largerIP, iph, thissession);
                    updateseq(largerIP, iph, tcph, thissession);

                    if ((remoteID == NULL) || verify_neighbor_in_domain(remoteID) == false) {// Accelerator ID was not found.
                        mms = __get_tcp_option((__u8 *)originalpacket,2);

                        if (mms > 60) {
                            __set_tcp_option((__u8 *)originalpacket,2,4,mms - 60); // Reduce the MSS.
                            //__set_tcp_option((__u8 *)originalpacket,30,6,localID); // Add the Accelerator ID to this packet.
                            set_nod_header_data((__u8 *)iph, ONOP, get_opennop_id(), OPENNOP_IPC_ID_LENGTH);
                            /*
                             * TCP Window Scale option seemed to break Win7 & Win8 Internet access.
                             */
                            //__set_tcp_option((__u8 *)originalpacket,3,3,G_SCALEWINDOW); // Enable window scale.

                            saveacceleratorid(largerIP, (char*)get_opennop_id(), iph, thissession);

                            /*
                             * Changing anything requires the IP and TCP
                             * checksum to need recalculated.
                             */
                            checksum(originalpacket);
                        }

                    } else if(verify_neighbor_in_domain(remoteID) == true) { // Accelerator ID was found and in domain.
                        saveacceleratorid(largerIP, remoteID, iph, thissession);

                    }

                    thissession->state = TCP_SYN_SENT;
                }

                /* Before we return let increment the packets counter. */
                thefetcher.metrics.packets++;

                /* This is the last step for a SYN packet. */
                /* accept all SYN packets. */
                return nfq_set_verdict(hq, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)originalpacket);

            } else { // Packet was not a SYN packet.
                thissession = getsession(largerIP, largerIPPort, smallerIP, smallerIPPort);

                if (thissession != NULL) {

                    /* Identify SYN/ACK packets that are part of a new */
                    /* session opening its connection. */
                    if ((tcph->syn == 1) && (tcph->ack == 1)) {
                        updateseq(largerIP, iph, tcph, thissession);

                        if ((remoteID == NULL) || verify_neighbor_in_domain(remoteID) == false) { // Accelerator ID was not found.
                            mms = __get_tcp_option((__u8 *)originalpacket,2);

                            if (mms > 60) {
                                __set_tcp_option((__u8 *)originalpacket,2,4,mms - 60); // Reduce the MSS.
                                //__set_tcp_option((__u8 *)originalpacket,30,6,localID); // Add the Accelerator ID to this packet.
                                set_nod_header_data((__u8 *)iph, ONOP, get_opennop_id(), OPENNOP_IPC_ID_LENGTH);
                                /*
                                 * TCP Window Scale option seemed to break Win7 & Win8 Internet access.
                                 */
                                //__set_tcp_option((__u8 *)originalpacket,3,3,G_SCALEWINDOW); // Enable window scale.

                                saveacceleratorid(largerIP, (char*)get_opennop_id(), iph, thissession);

                            }

                        } else if(verify_neighbor_in_domain(remoteID) == true) { // Accelerator ID was found and in domain.
                            saveacceleratorid(largerIP, remoteID, iph, thissession);

                        }

                        thissession->state = TCP_ESTABLISHED;

                        /*
                         * Changing anything requires the IP and TCP
                         * checksum to need recalculated.
                         */
                        checksum(originalpacket);

                        /* Before we return let increment the packets counter. */
                        thefetcher.metrics.packets++;
                        return nfq_set_verdict(hq, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)originalpacket);
                    }

                    /* This is session traffic of an active session. */
                    /* This packet will be placed in a queue to be processed */
                    if (DEBUG_FETCHER == true) {
                        sprintf(message, "Fetcher: Sending the packet to a queue.\n");
                        logger(LOG_INFO, message);
                    }

                    if (DEBUG_FETCHER == true) {
                        sprintf(message, "Fetcher: Packet ID: %u.\n", id);
                        logger(LOG_INFO, message);
                    }
                    thispacket = get_freepacket_buffer();

                    if (thispacket != NULL) {
                        save_packet(thispacket,hq, id, ret, (__u8 *)originalpacket, thissession);

                        if ((remoteID == NULL) || verify_neighbor_in_domain(remoteID) == false) {
                            optimize_packet(thissession->queue, thispacket);

                        } else if(verify_neighbor_in_domain(remoteID) == true) {
                            deoptimize_packet(thissession->queue, thispacket);
                        }
                    } else {

                        sprintf(message, "Fetcher: Failed getting packet buffer for optimization.\n");
                        logger(LOG_INFO, message);
                    }
                    /* Before we return let increment the packets counter. */
                    thefetcher.metrics.packets++;
                    return 0;
                } else { // Session does not exist check if it is being tracked by another Accelerator.

                    if (DEBUG_FETCHER == true) {
                        sprintf(message, "Fetcher: The session manager did not find a session.\n");
                        logger(LOG_INFO, message);
                    }

                    /* We only want to create new sessions for active sessions. */
                    /* This means we exclude anything accept ACK packets. */

                    if ((tcph->syn == 0) && (tcph->ack == 1) && (tcph->fin == 0)) {

                        if (remoteID != 0) { // Detected remote Accelerator its safe to add this session.
                            thissession = insertsession(largerIP, largerIPPort, smallerIP, smallerIPPort); // Insert into sessions list.

                            if (thissession != NULL) { // Test to make sure the session was added.
                                thissession->state = TCP_ESTABLISHED;

                                if(verify_neighbor_in_domain(remoteID) == true) {
                                    saveacceleratorid(largerIP, remoteID, iph, thissession);

                                } else {
                                    //__set_tcp_option((__u8 *)originalpacket,30,6,localID); // Overwrite the Accelerator ID to this packet.
                                	set_nod_header_data((__u8 *)iph, ONOP, get_opennop_id(), OPENNOP_IPC_ID_LENGTH);
                                    saveacceleratorid(largerIP, (char*)get_opennop_id(), iph, thissession);
                                }

                                thispacket = get_freepacket_buffer();

                                if (thispacket != NULL) {
                                    save_packet(thispacket,hq, id, ret, (__u8 *)originalpacket, thissession);
                                    deoptimize_packet(thissession->queue, thispacket);

                                } else {
                                    sprintf(message, "Fetcher: Failed getting packet buffer for deoptimization.\n");
                                    logger(LOG_INFO, message);
                                }
                                /* Before we return let increment the packets counter. */
                                thefetcher.metrics.packets++;
                                return 0;
                            }
                        }
                    }
                    /* Before we return let increment the packets counter. */
                    thefetcher.metrics.packets++;
                    return nfq_set_verdict(hq, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)originalpacket);
                }
            }
        } else { /* Packet was not a TCP Packet or ID was 0. */
            /* Before we return let increment the packets counter. */
            thefetcher.metrics.packets++;
            return nfq_set_verdict(hq, id, NF_ACCEPT, 0, NULL);
        }
    } else { /* Daemon is not in a running state so return packets. */

        if (DEBUG_FETCHER == true) {
            sprintf(message, "Fetcher: The service is not running.\n");
            logger(LOG_INFO, message);
        }
        /* Before we return let increment the packets counter. */
        thefetcher.metrics.packets++;
        return nfq_set_verdict(hq, id, NF_ACCEPT, 0, NULL);
    }
    /*
     * If we get here there was a major problem.
     */

    /* Before we return let increment the packets counter. */
    //thefetcher.metrics.packets++; //Removed dead code.

    return 0;
}

void *fetcher_function(void *dummyPtr) {
    long sys_pagesofmem = 0; // The pages of memory in this system.
    long sys_pagesize = 0; // The size of each page in bytes.
    long sys_bytesofmem = 0; // The total bytes of memory in the system.
    long nfqneededbuffer = 0; // Store how much memory the NFQ needs.
    long nfqlength = 0;
    int rv = 0;
    char buf[BUFSIZE]
    __attribute__ ((aligned));
    char message[LOGSZ];

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Initialzing opening library handle.\n");
        logger(LOG_INFO, message);
    }

    h = nfq_open();

    if (!h) {

        if (DEBUG_FETCHER == true) {
            sprintf(message,
                    "Fetcher: Initializing error opening library handle.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message,
                "Fetcher: Initializing un-binding existing nf_queue for AF_INET.\n");
        logger(LOG_INFO, message);
    }

    if (nfq_unbind_pf(h, AF_INET) < 0) {

        if (DEBUG_FETCHER == true) {
            sprintf(message, "Fetcher: Initializing error un-binding nf_queue.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Initializing binding to nf_queue.\n");
        logger(LOG_INFO, message);
    }

    if (nfq_bind_pf(h, AF_INET) < 0) {

        if (DEBUG_FETCHER == true) {
            sprintf(message,
                    "Fetcher: Initializing error binding to nf_queue.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Initializing binding to queue '0'.\n");
        logger(LOG_INFO, message);
    }
    qh = nfq_create_queue(h, 0, &fetcher_callback, NULL);

    if (!qh) {

        if (DEBUG_FETCHER == true) {
            sprintf(message,
                    "Fetcher: Initializing error binding to queue '0'.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Initializing setting copy mode.\n");
        logger(LOG_INFO, message);
    }

    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, BUFSIZE) < 0) { // range/BUFSIZE was 0xffff

        if (DEBUG_FETCHER == true) {
            sprintf(message, "Fetcher: Initializing error setting copy mode.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Initializing setting queue length.\n");
        logger(LOG_INFO, message);
    }

    sys_pagesofmem = sysconf(_SC_PHYS_PAGES);
    sys_pagesize = sysconf(_SC_PAGESIZE);

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: There are %li pages of memory.\n",
                sys_pagesofmem);
        logger(LOG_INFO, message);
        sprintf(message, "Fetcher: There are %li bytes per page.\n",
                sys_pagesize);
        logger(LOG_INFO, message);
    }

    if ((sys_pagesofmem <= 0) || (sys_pagesize <= 0)) {

        if (DEBUG_FETCHER == true) {
            sprintf(message,
                    "Fetcher: Initializing error failed checking system memory.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }

    sys_bytesofmem = (sys_pagesofmem * sys_pagesize);
    nfqneededbuffer = (sys_bytesofmem / 100) * 10;

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: NFQ needs %li bytes of memory.\n",
                nfqneededbuffer);
        logger(LOG_INFO, message);
    }
    nfqlength = nfqneededbuffer / BUFSIZE;

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: NFQ lenth will be %li.\n", nfqlength);
        logger(LOG_INFO, message);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: NFQ cache  will be %ld.MB\n", ((nfqlength
                * 2048) / 1024) / 1024);
        logger(LOG_INFO, message);
    }

    if (nfq_set_queue_maxlen(qh, nfqlength) < 0) {

        if (DEBUG_FETCHER == true) {
            sprintf(message,
                    "Fetcher: Initializing error setting queue length.\n");
            logger(LOG_INFO, message);
        }
        exit(EXIT_FAILURE);
    }
    nfnl_rcvbufsiz(nfq_nfnlh(h), nfqlength * BUFSIZE);
    fd = nfq_fd(h);

    register_counter(counter_updatefetchermetrics, (t_counterdata)
                     & thefetcher.metrics);

    while ((servicestate >= RUNNING) && ((rv = recv(fd, buf, sizeof(buf), 0))
                                         && rv >= 0)) {

        if (DEBUG_FETCHER == true) {
            sprintf(message, "Fetcher: Received a packet.\n");
            logger(LOG_INFO, message);
        }

        /*
         * This will execute the callback_function for each ip packet
         * that is received into the Netfilter QUEUE.
         */
        nfq_handle_packet(h, buf, rv);
    }

    /*
     * At this point the system is down.
     * If this is due to rv = -1 we need to change the state
     * of the service to STOPPING to alert the other components
     * to begin shutting down because of the queue failure.
     */
    if (rv == -1) {
        servicestate = STOPPING;
        sprintf(message, "Fetcher: Stopping last rv value: %i.\n", rv);
        logger(LOG_INFO, message);
    }

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Stopping unbinding from queue '0'.\n");
        logger(LOG_INFO, message);
    }

    nfq_destroy_queue(qh);

#ifdef INSANE

    if (DEBUG_FETCHER == true) {
        sprintf(message, "Fetcher: Fatal unbinding from queue '0'.\n");
        logger(LOG_INFO, message);
    }
    nfa_unbind_pf(h, AF_INET);
#endif

    //if (DEBUG_FETCHER == true)
    //{
    sprintf(message, "Fetcher: Stopping closing library handle.\n");
    logger(LOG_INFO, message);
    //}
    nfq_close(h);
    return NULL;
}

void fetcher_graceful_exit() {
    nfq_destroy_queue(qh);
    nfq_close(h);
    close(fd);
}

void create_fetcher() {
    pthread_create(&thefetcher.t_fetcher, NULL, fetcher_function, (void *) NULL);
}

void rejoin_fetcher() {
    pthread_join(thefetcher.t_fetcher, NULL);
}

struct commandresult cli_show_fetcher(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result  = { 0 };
	char msg[MAX_BUFFER_SIZE] = { 0 };
    __u32 ppsbps;
    char bps[11];
    char col1[11];
    char col2[14];
    char col3[3];

    sprintf(msg, "------------------------\n");
    cli_send_feedback(client_fd, msg);
    sprintf(msg, "|  5 sec  |  fetcher   |\n");
    cli_send_feedback(client_fd, msg);
    sprintf(msg, "------------------------\n");
    cli_send_feedback(client_fd, msg);
    sprintf(msg, "|   pps   |     in     |\n");
    cli_send_feedback(client_fd, msg);
    sprintf(msg, "------------------------\n");
    cli_send_feedback(client_fd, msg);

    strcpy(msg, "");
    ppsbps = thefetcher.metrics.pps;
    bytestostringbps(bps, ppsbps);
    sprintf(col1, "| %-8u", ppsbps);
    strcat(msg, col1);

    ppsbps = thefetcher.metrics.bpsin;
    bytestostringbps(bps, ppsbps);
    sprintf(col2, "| %-11s", bps);
    strcat(msg, col2);

    sprintf(col3, "|\n");
    strcat(msg, col3);
    cli_send_feedback(client_fd, msg);

    sprintf(msg, "------------------------\n");
    cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

void counter_updatefetchermetrics(t_counterdata data) {
    struct fetchercounters *metrics;
    char message[LOGSZ];
    __u32 counter;

    if (DEBUG_FETCHER_COUNTERS == true) {
        sprintf(message, "Fetcher: Updating metrics!");
        logger(LOG_INFO, message);
    }

    metrics = (struct fetchercounters*) data;
    counter = metrics->packets;
    metrics->pps = calculate_ppsbps(metrics->packetsprevious, counter);
    metrics->packetsprevious = counter;

    counter = metrics->bytesin;
    metrics->bpsin = calculate_ppsbps(metrics->bytesinprevious, counter);
    metrics->bytesinprevious = counter;
}
