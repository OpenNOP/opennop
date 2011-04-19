static int fetcher_callback(struct nfq_q_handle *hq, struct nfgenmsg *nfmsg,
			  struct nfq_data *nfa, void *data)
{
	u_int32_t id = 0;
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	struct session *thissession = NULL;
	struct nfqnl_msg_packet_hdr *ph;
	struct timeval tv;
	__u32 largerIP, smallerIP, acceleratorID;
	__u16 largerIPPort, smallerIPPort, mms;	
	int ret;
	char *originalpacket = NULL;
	char message [256];
	char strIP [20];
	//struct packet *newpacket = NULL;
	
	ph = nfq_get_msg_packet_hdr(nfa);
	
	if (ph) {
		id = ntohl(ph->packet_id);
	}
		
	ret = nfq_get_payload(nfa, &originalpacket);
	
	if (servicestate >= RUNNING){
		iph = (struct iphdr *) originalpacket;
		
		/* We need to double check that only TCP packets get accelerated. */
		/* This is because we are working from the Netfilter QUEUE. */
		/* User could QUEUE UDP traffic, and we cannot accelerate UDP. */
		if ((iph->protocol == IPPROTO_TCP) && (id != 0)){

	
			tcph = (struct tcphdr *) (((u_int32_t *)originalpacket) + iph->ihl);
			
			/* Check what IP address is larger. */
			sort_sockets(&largerIP, &largerIPPort, &smallerIP, &smallerIPPort,
				iph->saddr,tcph->source,iph->daddr,tcph->dest);
							
			acceleratorID = (__u32)__get_tcp_option((__u8 *)originalpacket,30);
			
			if (DEBUG_FETCHER == TRUE){
				inet_ntop(AF_INET, &acceleratorID, strIP, INET_ADDRSTRLEN);
				sprintf(message, "Fetcher: The accellerator ID is:%s.\n", strIP);
				logger(message);
			}
			
			/* Check if this a SYN packet to identify a new session. */
			/* This packet will not be placed in a work queue, but  */
			/* will be accepted here because it does not have any data. */	
			if ((tcph->syn == 1) && (tcph->ack == 0)){
				thissession = getsession(largerIP, largerIPPort, smallerIP, smallerIPPort); // Check for an outstanding syn.
					
					if (thissession == NULL){
						thissession = insertsession(largerIP, largerIPPort, smallerIP, smallerIPPort); // Insert into sessions list.
					}
				
				/* We need to check for NULL to make sure */
				/* that a record for the session was created */
				if (thissession != NULL){
					
					if (DEBUG_FETCHER == TRUE){
						sprintf(message, "Fetcher: The session manager created a new session.\n");
						logger(message);
					}
					
					gettimeofday(&tv,NULL); // Get the time from hardware.
					thissession->lastactive = tv.tv_sec; // Update the session timestamp.
					
					if (iph->saddr == largerIP){ // See what IP this is coming from.
		 			
		 				if (ntohl(tcph->seq) != (thissession->largerIPseq - 1)){			 					
		 					thissession->largerIPStartSEQ = ntohl(tcph->seq);
		 				}
		 			}
		 			else{
		 				
		 				if (ntohl(tcph->seq) != (thissession->smallerIPseq - 1)){	
		 					thissession->smallerIPStartSEQ = ntohl(tcph->seq);
		 				}
		 			}
					
					if (acceleratorID == 0){ // Accelerator ID was not found.
						mms = __get_tcp_option((__u8 *)originalpacket,2);
						
						if (mms > 60){
							__set_tcp_option((__u8 *)originalpacket,2,4,mms - 60); // Reduce the MSS.
							__set_tcp_option((__u8 *)originalpacket,30,6,localIP); // Add the Accelerator ID to this packet.
							
							if (iph->saddr == largerIP){ // Set the Accelerator for this source.
		 							thissession->largerIPAccelerator = localIP;
		 		 			}
		 		 			else{
		 		 				thissession->smallerIPAccelerator = localIP;
		 		 			}
		 		 			
		 		 			/*
		 		 			 * Changing anything requires the IP and TCP
		 		 			 * checksum to need recalculated.
		 		 			 */
		 		 			 checksum(originalpacket);
						}
					}
					else{ // Accelerator ID was found.
						
						if (iph->saddr == largerIP){ // Set the Accelerator for this source.
		 		 			thissession->largerIPAccelerator = acceleratorID;
		 		 		}
		 		 		else{
		 		 			thissession->smallerIPAccelerator = acceleratorID;
		 		 		}
					}
					
					thissession->state = TCP_SYN_SENT;
				}
				
				/* This is the last step for a SYN packet. */
				/* accept all SYN packets. */
				return nfq_set_verdict(hq, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)originalpacket);
			}
			else{ // Packet was not a SYN packet.
				thissession = getsession(largerIP, largerIPPort, smallerIP, smallerIPPort);
				
				if (thissession != NULL){
					gettimeofday(&tv,NULL); // Get the time from hardware.
					thissession->lastactive = tv.tv_sec; // Update the active timer.
		 			thissession->deadcounter = 0; // Reset the dead counter.
					
					if (iph->saddr == largerIP){ // See what IP this is coming from.
		 			
		 				if (ntohl(tcph->seq) != (thissession->largerIPseq - 1)){			 					
		 					thissession->largerIPseq = ntohl(tcph->seq);
		 				}
		 			}
		 			else{
		 				
		 				if (ntohl(tcph->seq) != (thissession->smallerIPseq - 1)){	
		 					thissession->smallerIPseq = ntohl(tcph->seq);
		 				}
		 			}

					/* Identify SYN/ACK packets that are part of a new */
					/* session opening its connection. */
					if ((tcph->syn == 1) && (tcph->ack == 1)){
						
						if (iph->saddr == largerIP){ // See what IP this is coming from.
		 			
		 					if (ntohl(tcph->seq) != (thissession->largerIPseq - 1)){			 					
		 						thissession->largerIPStartSEQ = ntohl(tcph->seq);
		 					}
		 				}
		 				else{
		 				
		 					if (ntohl(tcph->seq) != (thissession->smallerIPseq - 1)){	
		 						thissession->smallerIPStartSEQ = ntohl(tcph->seq);
		 					}
		 				}  
						
						if (acceleratorID == 0){ // Accelerator ID was not found.
							mms = __get_tcp_option((__u8 *)originalpacket,2);
						
							if (mms > 60){
								__set_tcp_option((__u8 *)originalpacket,2,4,mms - 60); // Reduce the MSS.
								__set_tcp_option((__u8 *)originalpacket,30,6,localIP); // Add the Accelerator ID to this packet.
							
								if (iph->saddr == largerIP){ // Set the Accelerator for this source.
		 							thissession->largerIPAccelerator = localIP;
		 		 				}
		 		 				else{
		 		 					thissession->smallerIPAccelerator = localIP;
		 		 				}
							}
						}
						else{ // Accelerator ID was found.
							
							if (iph->saddr == largerIP){ // Set the Accelerator for this source.
		 		 				thissession->largerIPAccelerator = acceleratorID;
		 		 			}
		 		 			else{
		 		 				thissession->smallerIPAccelerator = acceleratorID;
		 		 			}
						}
						thissession->state = TCP_ESTABLISHED;
						
						/*
		 		 		 * Changing anything requires the IP and TCP
		 		 		 * checksum to need recalculated.
		 		 		 */
		 		 		 checksum(originalpacket);
						return nfq_set_verdict(hq, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)originalpacket);
					}
					
						/* This is session traffic of an active session. */
						/* This packet will be placed in a queue to be processed */
						if (DEBUG_FETCHER == TRUE){
							sprintf(message, "Fetcher: Sending the packet to a queue.\n");
							logger(message);
						}
						
						if (DEBUG_FETCHER == TRUE){
							sprintf(message, "Fetcher: Packet ID: %u.\n", id);
							logger(message);
						}
						
						queue_packet(hq, id, ret, (__u8 *)originalpacket, thissession);
						return 0;	 
				}
				else{ // Session does not exist check if it is being tracked by another Accelerator.
					
					if (DEBUG_FETCHER == TRUE){
						sprintf(message, "Fetcher: The session manager did not find a session.\n");
						logger(message);
					}
					
					/* We only want to create new sessions for active sessions. */
					/* This means we exclude anything accept ACK packets. */
					
					if ((tcph->syn == 0) && (tcph->ack == 1) && (tcph->fin == 0)){
					
						if (acceleratorID != 0){ // Detected remote Accelerator its safe to add this session.
							thissession = insertsession(largerIP, largerIPPort, smallerIP, smallerIPPort); // Insert into sessions list.
						
							if (thissession != NULL){ // Test to make sure the session was added.
								thissession->state = TCP_ESTABLISHED;
							
								if (iph->saddr == largerIP){ // Set the Accelerator for this source.
		 		 					thissession->largerIPAccelerator = acceleratorID;
		 		 				}
		 		 				else{
		 		 					thissession->smallerIPAccelerator = acceleratorID;
		 		 				}
		 		 				queue_packet(hq, id, ret, (__u8 *)originalpacket, thissession);
		 		 				return 0;
							}
						}
					}
					return nfq_set_verdict(hq, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)originalpacket);
				}
			}
		}
		else{ /* Packet was not a TCP Packet or ID was 0. */
			return nfq_set_verdict(hq, id, NF_ACCEPT, 0, NULL);
		}	 
	}
	else{ /* Daemon is not in a running state so return packets. */
		
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: The service is not running.\n");
		logger(message);
		}
		return nfq_set_verdict(hq, id, NF_ACCEPT, 0, NULL);
	}
	 return 0;	 
}

void *fetcher_function (void *dummyPtr)
{
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	long sys_pagesofmem = 0; // The pages of memory in this system.
	long sys_pagesize = 0; // The size of each page in bytes.
	long sys_bytesofmem = 0; // The total bytes of memory in the system.
	long nfqneededbuffer = 0; // Store how much memory the NFQ needs.
	long nfqlength = 0;
	int fd;
	int rv;
	char buf[BUFSIZE] __attribute__ ((aligned));
	char message [256];
	
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Initialzing opening library handle.\n");
		logger(message);
	}
	
	h = nfq_open();
	
	if (!h){
		
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error opening library handle.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}
        
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Initialzing unbinding existing nf_queue for AF_INET.\n");
		logger(message);
	}
        
	if (nfq_unbind_pf(h, AF_INET) < 0){
        	
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error unbinding nf_queue.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}
        
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Initialzing binding to nf_queue.\n");
		logger(message);
	}
        
	if (nfq_bind_pf(h, AF_INET) < 0){
        	
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error binding to nf_queue.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}
		
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Initialzing binding to queue '0'.\n");
		logger(message);
	}
	qh = nfq_create_queue(h, 0, &fetcher_callback, NULL);
		
	if (!qh){
			
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error binding to queue '0'.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}
		
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Initialzing setting copy mode.\n");
		logger(message);
	}
		
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, BUFSIZE) < 0){ // range/BUFSIZE was 0xffff
			
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error setting copy mode.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}
		
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Initialzing setting queue length.\n");
		logger(message);
	}
	
	sys_pagesofmem = sysconf(_SC_PHYS_PAGES);
	sys_pagesize = sysconf(_SC_PAGESIZE);
	
	if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: There are %li pages of memory.\n", sys_pagesofmem);
			logger(message);
			sprintf(message, "Fetcher: There are %li bytes per page.\n", sys_pagesize);
			logger(message);
		}
	
	if ((sys_pagesofmem <= 0) || (sys_pagesize <= 0)) {
		
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error failed checking system memory.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}
	
	sys_bytesofmem = (sys_pagesofmem * sys_pagesize);
	nfqneededbuffer = (sys_bytesofmem / 100) * 10;
	
	if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: NFQ needs %li bytes of memory.\n", nfqneededbuffer);
		logger(message);
	}
	nfqlength = nfqneededbuffer / BUFSIZE;
		
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: NFQ lenth will be %li.\n", nfqlength);
			logger(message);
		}
		
		if (DEBUG_FETCHER == TRUE) {
			sprintf(message, "Fetcher: NFQ cache  will be %ld.MB\n", ((nfqlength * 2048) /1024) / 1024);
			logger(message);
		}
		
	if (nfq_set_queue_maxlen(qh, nfqlength) < 0) {
			
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Initialzing error setting queue length.\n");
			logger(message);
		}
		exit(EXIT_FAILURE);
	}	
	nfnl_rcvbufsiz(nfq_nfnlh(h), nfqlength * BUFSIZE);
	fd = nfq_fd(h);
		
	while ((servicestate >= STOPPING) && ((rv = recv(fd, buf, sizeof(buf), 0)) && rv >= 0)) {
		
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Received a packet.\n");
			logger(message);
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
	if (rv == -1){
		servicestate = STOPPING;
		sprintf(message, "Fetcher: Stopping last rv value: %i.\n", rv);
		logger(message);
	} 
	
	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Stopping unbinding from queue '0'.\n");
		logger(message);
	}
	


    nfq_destroy_queue(qh);
        
	#ifdef INSANE
		
		if (DEBUG_FETCHER == TRUE){
			sprintf(message, "Fetcher: Fatal unbinding from queue '0'.\n");
			logger(message);
		}
		nfa_unbind_pf(h, AF_INET);
	#endif

	if (DEBUG_FETCHER == TRUE){
		sprintf(message, "Fetcher: Stopping closing library handle.\n");
		logger(message);
	}
	nfq_close(h);
	return NULL;
}
