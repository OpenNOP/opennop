/* Original function from http://www.bloof.de/tcp_checksumming */
/* also from http://www.netfor2.com/tcpsum.htm */

unsigned short tcp_sum_calc(unsigned short len_tcp, unsigned short *src_addr, unsigned short *dest_addr, unsigned short *buff)
{
	struct tcphdr *tcph;
	unsigned short prot_tcp = 6;
	long sum = 0;
	int i = 0;
	tcph = (struct tcphdr *)buff;
	
	/* add the pseudo header */	
	sum += ntohs(src_addr[0]);
	sum += ntohs(src_addr[1]);
	sum += ntohs(dest_addr[0]);
	sum += ntohs(dest_addr[1]);
	sum += len_tcp; // already in host format.
	sum += prot_tcp; // already in host format.
      
	/* 
	 * calculate the checksum for the tcp header and payload
	 * len_tcp represents number of 8-bit bytes, 
	 * we are working with 16-bit words so divide len_tcp by 2. 
	 */
	for(i=0;i<(len_tcp/2);i++){
		sum += ntohs(buff[i]);
	}
	
	/* Check if the tcp length is even or odd.  Add padding if odd. */
	if((len_tcp % 2) == 1){
		sum += ntohs((unsigned char)(buff[i]));	
	}
    
	// keep only the last 16 bits of the 32 bit calculated sum and add the carries
	while (sum >> 16){
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	// Take the bitwise complement of sum
	sum = ~sum;

	return htons(((unsigned short) sum));
}

unsigned short ip_sum_calc(unsigned short len_ip_header, unsigned short *buff){
	long sum = 0;
	int i = 0;
	
	for (i=0;i<len_ip_header/2;i++){
		sum += ntohs(buff[i]);
	}
	
	while (sum >> 16){
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	sum = ~sum;
	
	return htons(((unsigned short) sum));
}

void checksum(unsigned char *packet)
{
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	__u16 tcplen;
	
	iph = (struct iphdr *) packet;
		
		/* We only need to checksum TCP packets. */
		if (iph->protocol == IPPROTO_TCP){

			tcph = (struct tcphdr *) (((u_int32_t *)packet) + iph->ihl);
			tcplen = ntohs(iph->tot_len) - iph->ihl*4;
			tcph->check = 0;
			tcph->check = tcp_sum_calc(tcplen,
				(unsigned short *)&iph->saddr,
				(unsigned short *)&iph->daddr,
				(unsigned short *)tcph);
 			iph->check = 0;
 				iph->check = ip_sum_calc(iph->ihl*4,
 				(unsigned short *)iph);
		}
}
