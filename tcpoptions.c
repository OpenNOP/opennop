#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include "tcpoptions.h"
#include "logger.h"

int DEBUG_TCPOPTIONS = false;

__u8 optlen(const __u8 *opt, __u8 offset){
	if (opt[offset] <= TCPOPT_NOP || opt[offset+1] == 0)
		return 1;
	else
		return opt[offset+1];
}

/*
 * yaplej: This function will attempt to locate
 * the requested tcp option from the passed skb.
 *
 * If it locates the tcp option it will then check its length
 * to determine if there is any option data to return.
 * If there is option data it will return that, and
 * if there is not data for the option it will return 1.
 *
 * If the option is not found it will return 0.
 */
__u64 __get_tcp_option(__u8 *ippacket, __u8 tcpoptnum){
	struct iphdr *iph;
	struct tcphdr *tcph;
	__u64 tcpoptdata;
	__u8 i, tcpoptlen, bytefield, count, *opt;

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	for (i = 0; i < tcph->doff*4 - sizeof(struct tcphdr); i += optlen(opt, i)) {
		
		if (opt[i] == tcpoptnum) {

			tcpoptlen = opt[i+1]; // get option length.

			if ((tcph->doff*4 - i >= tcpoptlen) &&
				(opt[i+1] == tcpoptlen)) {
					
				if (tcpoptlen == 2){ // TCP Option was found, but no data.
					return 1;
				}
				else{
					count = tcpoptlen - 2; // get option length,
										  // and ignore header fields.
					bytefield = 2;  // the first data byte is always at i+2.
					tcpoptdata = 0; // initialize tcpoptdata for compiler.

					while (count > 0){
						count--;
						
						//if ((count) != 0) {
							tcpoptdata += (opt[i+bytefield] << 8 * count);
						//}
						//else {
						//	tcpoptdata += opt[i+bytefield];
						//}

						bytefield++;
					}
				return tcpoptdata;
				}
			}
		}
	}
	return 0;
}

#include "tcpoptions.h"

/*
 * yaplej: This function will attempt to update,
 * or add any specific tcp option.  By passing the
 * skb, option number, option length in byte, and
 * any data into the tcp segment.
 */
int __set_tcp_option(__u8 *ippacket, unsigned int tcpoptnum,
 unsigned int tcpoptlen, u_int64_t tcpoptdata){
	struct tcphdr *tcph;
	struct iphdr *iph;
	__u16 tcplen;
	__u8 i, optspace, addoff, spaceneeded, bytefield, count, *opt;
	char message [LOGSZ];

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	if (tcph->doff > 5){
	
		for (i = 0; i < tcph->doff*4 - sizeof(struct tcphdr); i += optlen(opt, i)) {

			if ((opt[i] == tcpoptnum) && (tcph->doff*4 - i >= tcpoptlen) &&
				(opt[i+1] == tcpoptlen)) { // TCP Option was found.
			
				count = tcpoptlen - 2; // Get option data length,
				bytefield = 2;  // First data byte is always at i+2.

				while (count > 0) {
					count--;	
					opt[i+bytefield] = (tcpoptdata  >> 8 * count);
					bytefield++;
				}

				// Get the TCP length.
				tcplen = ntohs(iph->tot_len) - iph->ihl*4;
			
				return 0;
			}
		}
 	}
 	
	// TCP Option was not found!

	optspace = 0;
	if (tcph->doff > 5){
			
		
		// Remove TCPOPT_NOP, and find any free space in opt field.
		for (i = 0; i < tcph->doff*4 - sizeof(struct tcphdr); i += optlen(opt, i)) {

			// While TCP option space = TCPOPT_NOP.
			while (opt[i] == TCPOPT_NOP){
					
				// Move options forward to use TCPOPT_NOP space.
				memmove(opt + i,opt +i + 1,(((tcph->doff*4) - sizeof(struct tcphdr)) -i) -1);
				opt[(((tcph->doff*4)- sizeof(struct tcphdr)) -1)] = 0;
			}
				
			// If TCP option = TCPOPT_EOL its the end of TCP options.
			if (opt[i] == TCPOPT_EOL){
					
				optspace = ((tcph->doff*4) - sizeof(struct tcphdr)) -i;
				break;
			}
			else{ // End of option space not found
					
				optspace = 0;
			}
		}
	}

	addoff = 0;
	spaceneeded = 0;
	
	if (optspace < tcpoptlen){

		// Calculate the new space required to add this option.
		spaceneeded = tcpoptlen - optspace;

		// Calculate how many dwords need added (data offset must align on dwords)
		addoff = spaceneeded/4;


		// Odd number of bytes needed increase by one.
		if (spaceneeded%4 != 0) {
			addoff += 1;
		}
	}

	if (tcph->doff + addoff <= 15){

		if (addoff != 0) {
	
			// Get old TCP length.
			tcplen = ntohs(iph->tot_len) - iph->ihl*4;
				
				if (DEBUG_TCPOPTIONS == true){
					sprintf(message, "TCP Options: The old tcp length is %d!\n",tcplen);
					logger(LOG_INFO, message);
				}
				
				if (DEBUG_TCPOPTIONS == true){
					sprintf(message, "TCP Options: The addoff is %d!\n",addoff);
					logger(LOG_INFO, message);
				}
			
			
			

			// Moving tcp data back to new location.
			memmove(((opt + tcph->doff*4) - sizeof(struct tcphdr)) + addoff*4,
				((opt + tcph->doff*4) - sizeof(struct tcphdr)),
				((ntohs(iph->tot_len) - iph->ihl*4) - tcph->doff*4));

			// Zero space between old, and new data offsets.
			for (i = 0; i < addoff*4; i++) {
				opt[((tcph->doff*4)- sizeof(struct tcphdr))+i] = 0;
			}
		}


		// Moving options back to make room for new tcp option.
		memmove(opt + tcpoptlen,
				opt,
				(tcph->doff*4 - sizeof(struct tcphdr)) - optspace);

		count = tcpoptlen - 2; // Get option data length.		
		bytefield = 2; // First data byte is always at i+2.

		// Writing new option to freed space.
		opt[0] = tcpoptnum;
		opt[1] = tcpoptlen;

		while (count > 0){ //Writing TCP option data.
			count--;
				
			//if ((count) != 0){
				opt[bytefield] = (tcpoptdata  >> 8 * count);
			//}
			//else{
			//	opt[bytefield] = tcpoptdata; //& 0x00ff;
			//}

			bytefield++;
		}

		if (addoff != 0){ // Fixing data offset.
			tcph->doff += addoff; 
		}

		// Fix packet length.
		if (DEBUG_TCPOPTIONS == true){
			sprintf(message, "TCP Options: Old IP packet length is %d!\n",ntohs(iph->tot_len));
			logger(LOG_INFO, message);
		}
		
		iph->tot_len = htons(ntohs(iph->tot_len) + addoff*4);
		
		if (DEBUG_TCPOPTIONS == true){
			sprintf(message, "TCP Options: New IP packet length is %d!\n",ntohs(iph->tot_len));
			logger(LOG_INFO, message);
		}

		
		// Get new TCP length.
		tcplen = ntohs(iph->tot_len) - iph->ihl*4;
		if (DEBUG_TCPOPTIONS == true){
			sprintf(message, "TCP Options: New TCP segment length is %d!\n",tcplen);
			logger(LOG_INFO, message);
		}
		

		return 0;
	}
	else{
			
		return -1;
	}

}
