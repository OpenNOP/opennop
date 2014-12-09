#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include "tcpoptions.h"
#include "logger.h"

#define	NOD	33 //* TCP Option # used by Network Optimization Detection.
#define NOD_MIN_LENGTH 2
#define ONOP_COMPRESSION 1 //* OpenNOP NOD Data containing Compression information.  0 = Uncompressed, 1 = Compressed.

int DEBUG_TCPOPTIONS = false;
static int DEBUG_NOD = LOGGING_OFF;

__u8 optlen(const __u8 *opt, __u8 offset){
	if (opt[offset] <= TCPOPT_NOP || opt[offset+1] == 0)
		return 1;
	else
		return opt[offset+1];
}

void remove_tcpopt_nop(__u8 *ippacket){
	struct tcphdr *tcph;
	struct iphdr *iph;
	__u8 i, *opt;
	char message[LOGSZ] = {0};

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);

	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	if (tcph->doff > 5){

		// Remove TCPOPT_NOP
		for (i = 0; i < tcph->doff*4 - sizeof(struct tcphdr); i += optlen(opt, i)) {

			// While TCP option space = TCPOPT_NOP.
			while (opt[i] == TCPOPT_NOP){
				sprintf(message, "[NOD] Removing TCPOPT_NOP.\n");
				logger2(LOGGING_ERROR,DEBUG_NOD,message);
				// Move options forward to use TCPOPT_NOP space.
				memmove(opt + i,opt +i + 1,(((tcph->doff*4) - sizeof(struct tcphdr)) -i) -1);
				opt[(((tcph->doff*4)- sizeof(struct tcphdr)) -1)] = 0;
			}
		}
	}
}

__u8 *get_tcpopt(__u8 *ippacket, __u8 tcpoptionnum){
	struct tcphdr *tcph;
	struct iphdr *iph;
	__u8 i, *opt;
	char message[LOGSZ] = {0};

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	for (i = 0; i < tcph->doff*4 - sizeof(struct tcphdr); i += optlen(opt, i)) {

		if (opt[i] == tcpoptionnum) {
			return &opt[i];
		}
	}

	return NULL;
}

__u8 get_tcpopt_freespace(__u8 *ippacket){
	struct tcphdr *tcph;
	struct iphdr *iph;
	__u8 i, optspace, *opt;
	char message [LOGSZ];

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	remove_tcpopt_nop(ippacket);
	optspace = 0;

	if (tcph->doff > 5){

		for (i = 0; i < tcph->doff*4 - sizeof(struct tcphdr); i += optlen(opt, i)) {

			// If TCP option = TCPOPT_EOL its the end of TCP options.
			if (opt[i] == TCPOPT_EOL){

				optspace = ((tcph->doff*4) - sizeof(struct tcphdr)) -i;
				break;
			}
		}
	}

	return optspace;
}

/**
 * @brief Returns the number of DWORDS (4 bytes) of space needed to append a given number of bytes to the TCP Options.
 *
 * @param ippacket   [in] Pointer to the IP packet being modified.
 * @param bytestoadd [in] Number of bytes that you want to add to TCP Options.
 */
__u8 get_tcpopt_addoff_needed(__u8 *ippacket, __u8 bytestoadd){
	struct iphdr *iph;
	struct tcphdr *tcph;
	__u8 optspace, addoff, spaceneeded, *opt;
	char message [LOGSZ];

	optspace = 0;
	addoff = 0;
	spaceneeded = 0;
	opt = NULL;

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	optspace = get_tcpopt_freespace(ippacket);

	if (optspace < bytestoadd){

		// Calculate the new space required to add this option.
		spaceneeded = bytestoadd - optspace;

		// Calculate how many dwords need added (data offset must align on dwords)
		addoff = spaceneeded/4;

		// Odd number of bytes needed increase by one.
		if (spaceneeded%4 != 0) {
			addoff += 1;
		}
	}

	return addoff;
}

/**
 * @brief This increases the TCP Option space by # of WORDS and moves the TCP data back accordingly.
 *
 * @param ippacket [in] Pointer to the IP packet being modified.
 * @param addoff   [in] Number of DWORDS TCP Option area is being increased by.
 */
void add_tcpopt_addoff(__u8 *ippacket, __u8 addoff){
	struct iphdr *iph;
	struct tcphdr *tcph;
	__u16 tcplen;
	__u8 i, *opt;

	tcplen = 0;
	opt = NULL;

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	if(addoff == 0){
		return;
	}

	//* If the original doff + this additional offset is > 15 we need to ERROR!/
	if (tcph->doff + addoff <= 15){
		// Get old TCP length.
		tcplen = ntohs(iph->tot_len) - iph->ihl*4;

		// Moving tcp data back to new location.
		memmove(((opt + tcph->doff*4) - sizeof(struct tcphdr)) + addoff*4,
			((opt + tcph->doff*4) - sizeof(struct tcphdr)),
			((ntohs(iph->tot_len) - iph->ihl*4) - tcph->doff*4));

		// Zero space between old, and new data offsets.
		for (i = 0; i < addoff*4; i++) {
			opt[((tcph->doff*4)- sizeof(struct tcphdr))+i] = 0;
		}

		// Fix the TCP offset & IP Length.
		tcph->doff += addoff;
		iph->tot_len = htons(ntohs(iph->tot_len) + addoff*4);

	}else{
		/*
		 * @todo:
		 * Need to return an error here because the required data cannot be added to the packet.
		 * Should we allow the packet through or drop it?
		 */
	}
}

__u8 *set_tcpopt(__u8 *ippacket, __u8 tcpoptionnum, __u8 tcpoptionlen){
	struct tcphdr *tcph;
	struct iphdr *iph;
	__u8 addoff, *opt;
	char message[LOGSZ] = {0};

	opt = NULL;
	opt = get_tcpopt(ippacket, tcpoptionnum);

	if(opt == NULL){
		iph = (struct iphdr *)ippacket;
		tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
		//sprintf(message, "[TCP] No TCP option found.\n");
		//logger(LOG_INFO, message);
		addoff = get_tcpopt_addoff_needed(ippacket, 2);
		add_tcpopt_addoff(ippacket,addoff);
		opt = get_tcpopt(ippacket,TCPOPT_EOL);

		if(opt != NULL){
			//sprintf(message, "[TCP] Found TCPOPT_EOL.\n");
			//logger(LOG_INFO, message);
			opt[0] = tcpoptionnum;
			opt[1] = tcpoptionlen;
		}
	}

	return opt;
}

int check_nod_header(struct nodhdr *nodh, const char *id){
	__u8 *iddata, i;

	iddata = (__u8*)&nodh->iddata;

	for(i=0; i < strlen(id); i++){

		if((__u8)id[i] != iddata[i]){
			return 0;
		}
	}

	return 1;
}

/**
 * @brief Finds a NOD header by its ID.
 *
 * @param ippacket [in] Pointer to the IP packet being modified.
 * @param id [in] Char array of the string used as ID for the NOD.
 */
struct nodhdr *get_nod_header(__u8 *ippacket, const char *id){
	__u8 *nod, *iddata;
	struct nodhdr *nodh;
	char message[LOGSZ] = {0};

	nod = get_tcpopt(ippacket, NOD);

	if(nod != NULL){
		if(nod[1] > 2){
			sprintf(message, "[TCPOPT] Found NOD headers.\n");
			logger(LOG_INFO, message);
			nodh = (struct nodhdr*)&nod[2];

			if(check_nod_header(nodh, id) == 1){
				sprintf(message, "[TCPOPT] Header is a match.\n");
				logger(LOG_INFO, message);

				return nodh;
			}

		}else{
			return NULL;
		}
	}

	return NULL;
}

/**
 * @brief Creates or sets the NOD header.
 *
 * @param ippacket [in] Pointer to the IP packet being modified.
 * @param id [in] Char array of the string used as ID for the NOD.
 * @param data [in] Pointer to the data being added to the NOD Option.
 * @param length [in] Length of the data being added.
 */
struct nodhdr *set_nod_header(__u8 *ippacket, const char *id){
	struct tcphdr *tcph;
	struct iphdr *iph;
	struct nodhdr *nodh;
	__u8 *nod, *opt, *iddata, *optstart, *optend, addoff, headerlen, i, bytestotcpoptend;
	char message[LOGSZ] = {0};

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = NULL;

	//opt = get_tcpopt(tcph, NOD);

	//if (opt == NULL) { //NOD Option not present.
		//sprintf(message, "[NOD] No Option.\n");
		//logger(LOG_INFO, message);
		//logger2(LOGGING_ERROR,DEBUG_NOD,message);
	//}

	//get_tcpopt_addoff_needed(ippacket, 24);
	//add_tcpopt_addoff(ippacket,2);

	nod = set_tcpopt(ippacket, NOD, 2);

	if(nod[1] == 2){ //* If the NOD Option length is 2 there are no headers and we must add a new one.
		//sprintf(message, "[NOD] Length is 2 no headers.\n");
		//logger(LOG_INFO, message);
		optstart = (__u8 *)tcph + sizeof(struct tcphdr);
		optend = get_tcpopt(ippacket,TCPOPT_EOL);
		sprintf(message, "[NOD] Optlen is: %li.\n",(optend - optstart));
		logger(LOG_INFO, message);

		headerlen = 2 + strlen(id);
		addoff = get_tcpopt_addoff_needed(ippacket,headerlen);
		sprintf(message, "[NOD] ADDOFF: %u.\n",addoff);
		logger(LOG_INFO, message);
		add_tcpopt_addoff(ippacket,addoff);

		//* There are 0 headers so we need to move any TCP Options back to make space for the new NOD header.
		//sprintf(message, "[NOD] Bytes to TCPOPT_EOL is: %li.\n",optend - &nod[2]);
		//logger(LOG_INFO, message);
		bytestotcpoptend = (optend - &nod[2]);

		if(bytestotcpoptend == 0){ // Already at the end of the TCP Option space so we can simply append data here.
			nod[1] = nod[1] += headerlen;
			nodh = (struct nodhdr*)&nod[2];
			nodh->len = headerlen;
			nodh->idlen = strlen(id);
			nodh->length = headerlen;
			iddata = &nodh->iddata;

			/*
			 * Write the NOD Header ID data.
			 */
			for(i=0; i < strlen(id); i++){
				iddata[i] = (__u8)id[i];
			}

			return nodh;

		}else{ // There are additional TCP Options after the NOD options so we must push that data back first.

		}


	}

	return NULL;
}

void set_nod_header_data(__u8 *ippacket, const char *id, __u8 *header_data, int header_data_length){
}

void set_nod_data(__u8 *ippacket, const char *id, __u8 data_header, __u8 *data, int data_length){
}

/**
 * @brief Removes a TCP Option from an IP packet.
 *
 * @param ippacket [in] Pointer to the IP packet being modified.
 * @param tcpoptionnum [in] TCP Option number being removed.
 *
 * @return int 1 = Success, 0 = Error
 */
int __del_tcp_option(__u8 *ippacket, __u8 tcpoptionnum){
	struct iphdr *iph;
	struct tcphdr *tcph;
	__u8 *opt;

	iph = (struct iphdr *)ippacket;
	tcph = (struct tcphdr *) (((u_int32_t *)ippacket) + iph->ihl);
	opt = (__u8 *)tcph + sizeof(struct tcphdr);

	return 0;
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
