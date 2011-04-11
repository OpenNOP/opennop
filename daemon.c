#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <syslog.h> // for error messages
#include <string.h>
#include <assert.h>
#include <signal.h> // for service signaling
#include <pthread.h> // for multi-threading
#include <unistd.h> // for sleep function
#include <ifaddrs.h> // for getting ip addresses
#include <netdb.h>
#include <time.h> // for session cleanup

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> // for sending keep-alives

#include <arpa/inet.h> // for getting local ip address

#include <netinet/in.h>
#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include <linux/types.h>
#include <linux/netfilter.h> // for NF_ACCEPT

#include <libnetfilter_queue/libnetfilter_queue.h> // for access to Netfilter Queue
#include <linux/genetlink.h> 

#define DAEMON_NAME "opennopd"
#define PID_FILE "/var/run/opennopd.pid"
#define MAXWORKERS 255 // Maximum number of workers to process packets.
#define BUFSIZE 2048 // Size of buffer used to store IP packets.
#define SESSIONBUCKETS 65536 // Number of buckets in the hash table for sessoin.
#define LOOPBACKIP 16777343UL // Loopback IP address 127.0.0.1.

/* Define states for worker threads. */
#define STOPPED -1
#define STOPPING 0
#define RUNNING 1

/* Define bool values. */
#define FALSE 0
#define TRUE 1

/* Structure used for the head of a session list. */
struct session_head {
	struct session *next; /* Points to the first session of the list. */
	struct session *prev; /* Points to the last session of the list. */
	u_int32_t len; // Total number of sessions in the list.
	pthread_mutex_t lock; // Lock for this session bucket.
};

/* Structure used to store TCP session info. */
struct session {
	struct session_head *head; // Points to the head of this list.
	struct session *next; // Points to the next session in the list.
	struct session *prev; // Points to the previous session in the list.
	__u32 largerIP; // Stores the larger IP address.
	__u16 largerIPPort; // Stores the larger IP port #.
	__u32 largerIPStartSEQ; // Stores the starting SEQ number.
	__u32 largerIPseq; // Stores the TCP SEQ from the largerIP.
	__u32 largerIPAccelerator; // Stores the AcceleratorIP of the largerIP.
	__u32 smallerIP; // Stores the smaller IP address.
	__u16 smallerIPPort; // Stores the smaller IP port #.
	__u32 smallerIPStartSEQ; // Stores the starting SEQ number.
	__u32 smallerIPseq; // Stores the TCP SEQ from the smallerIP.
	__u32 smallerIPAccelerator; // Stores the AcceleratorIP of the smallerIP.
	__u64 lastactive; // Stores the time this session was last active.
	__u8 deadcounter; // Stores how many counts the session has been idle.
	__u8 state; // Stores the TCP session state.
	__u8 queue; // What worker queue the packets for this session go to.
};

/* Structure used for the head of a packet queue.. */
struct packet_head{
	struct packet *next; // Points to the first packet of the list.
	struct packet *prev; // Points to the last packet of the list.
	u_int32_t qlen; // Total number of packets in the list.
	pthread_mutex_t lock; // Lock for this queue.
};

struct packet {
	struct packet_head *head; // Points to the head of this list.
	struct packet *next; // Points to the next packet.
	struct packet *prev; // Points to the previous packet.
	struct nfq_q_handle *hq; // The Queue Handle to the Netfilter Queue.
	u_int32_t id; // The ID of this packet in the Netfilter Queue.
	char data[BUFSIZE]; // Stores the actual IP packet.
};

/* Structure contains the worker thread, queue, and status. */
struct worker {
	pthread_t t_worker; // Is the thread for this worker.
	pthread_cond_t signal; // Condition signal used to wake-up thread.
	__u32 packets; // Number of packets this worker has processed.
	__u8 *lzbuffer; // Buffer used for LZ compression.
	__u8 *lzfastbuffer; // Buffer used for FastLZ compression.
	u_int32_t sessions; // Number of sessions assigned to the worker.
	struct packet_head queue; // Points to the queue for this worker.
	int state;	// Marks this thread as active. 1=running, 0=stopping, -1=stopped.
	pthread_mutex_t lock; // Lock for this worker.
};

/* Global Variables. */
int servicestate = RUNNING; // Current state of the service. 
int rawsock = 0; // Used to send keep-alive messages.
int healthtimer = 5; // Time in seconds between healthagent updates to the kernel module.  Kernel module check in 5 second increments.
struct worker workers[MAXWORKERS]; // setup slots for the max number of workers.
struct session_head sessiontable[SESSIONBUCKETS]; // Setup the session hashtable.
unsigned char numworkers = 0; // sets number of worker threads. 0 = auto detect.
__u32 localIP = 0; // Variable to store eth0 IP address used as the device ID.
int isdaemon = TRUE; // Determines how to log the messages and errors.

/* Debug Variables */
int DEBUG_FETCHER = FALSE; 
int DEBUG_HEALTHAGENT = FALSE;
int DEBUG_HEARTBEAT = FALSE;
int DEBUG_SESSIONMANAGER_INSERT = FALSE;
int DEBUG_SESSIONMANAGER_GET = FALSE;
int DEBUG_SESSIONMANAGER_REMOVE = FALSE;
int DEBUG_WORKER = FALSE;
int DEBUG_TCPOPTIONS = FALSE;

#include "utilities.h"
#include "queuemanager.h"
#include "lib/lz.h"
#include "csum.h"
#include "tcpoptions.h"
#include "compression.h"
#include "sessionmanager.h"
#include "sessioncleanup.h"
#include "subsystems/fetcher.h"
#include "help.h"
#include "signal.h"
#include "subsystems/worker.h"
#include "subsystems/healthagent.h"

int main(int argc, char *argv[])
{
	pthread_t t_fetcher; // thread for getting packets out of Netfilter Queue.
	pthread_t t_cleanup; // thread for cleaning up dead sessions.
	pthread_t t_healthagent; // thread for health agent. 
	struct ifaddrs *ifaddr, *ifa;
	__u32 tempIP;
	int s;
	int i;
	int one = 1;
	const int *val = &one;
	char message [256];
	char strIP [20];
	char host[NI_MAXHOST];

	#if defined(DEBUG)
		int daemonize = 0;
	#else
		int daemonize = 1;
	#endif
	
	/* Setup signal handling */
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	
	int c;
	while ( (c = getopt(argc, argv, "nh|help")) != -1) {
		switch(c){
			case 'h':
				PrintUsage(argc, argv);
				exit(0);
				break;
			case 'n':
				daemonize = 0;
				isdaemon = FALSE;
				break;
			default:
				PrintUsage(argc, argv);
				break;
		}
	}
	
	sprintf(message, "Initialization: %s daemon starting up.\n", DAEMON_NAME);
	logger(message);
	
	/*
	 * Get the numerically highest local IP address.
	 * This will be used as the acceleratorID.
	 */
	if (getifaddrs(&ifaddr) == -1){
		sprintf(message, "Initialization: Error opening interfaces.\n");
		logger(message);
		exit(EXIT_FAILURE);
	}
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next){  // loop through all interfaces.
		
		if (ifa->ifa_addr->sa_family == AF_INET){ // get all IPv4 addresses.
			s = getnameinfo(ifa->ifa_addr,
				sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
				
			if (s != 0){
				exit(EXIT_FAILURE);
			}
			
			tempIP = in_aton((char *)&host);  // convert string to decimal.
			
			/* 
			 * Lets fine the largest local IP, and use that as accelleratorID
			 * Lets also exclude 127.0.0.1 as a valid ID. 
			 */
			if ((tempIP > localIP) && (tempIP != LOOPBACKIP)) {
				localIP = tempIP;
			}
		} // end get all IPv4 addresses.
	} // end loop through all interfaces.
	
	if (localIP == 0){ // fail if no usable IP found.
		inet_ntop(AF_INET, &tempIP, strIP, INET_ADDRSTRLEN);
		sprintf(message, "Initialization: No usable IP Address. %s\n",strIP);
		logger(message);
		exit(EXIT_FAILURE);
	}
	
	rawsock = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
	
	if (rawsock < 0){
		sprintf(message, "Initialization: Error opening raw socket.\n");
		logger(message);
		exit(EXIT_FAILURE);
	}
	
	if(setsockopt(rawsock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0){
		sprintf(message, "Initialization: Error setting socket options.\n");
		logger(message);
		exit(EXIT_FAILURE);
	}

	#if defined(DEBUG)
		setlogmask(LOG-UPTO(LOG_DEBUG));
		openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
	#else
		setlogmask(LOG_UPTO(LOG_INFO));
		openlog(DAEMON_NAME, LOG_CONS, LOG_USER);
	#endif
        
    	/* Our process ID and Session ID */
        pid_t pid, sid;
        if (daemonize) {
        	sprintf(message, "Initialization: Daemonizing the %s process.\n", DAEMON_NAME);
			logger(message);
        	
        	/* Fork off the parent process */
        	pid = fork();
        	if (pid < 0)         	{
                exit(EXIT_FAILURE);
        	}
        	
        	/* If we got a good PID, then
           	   we can exit the parent process. */
        	if (pid > 0) {
                exit(EXIT_SUCCESS);
        	}

        	/* Change the file mode mask */
        	umask(0);
                
        	/* Create a new SID for the child process */
        	sid = setsid();
        	if (sid < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        	}
        
        	/* Change the current working directory */
        	if ((chdir("/")) < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        	}
        
        	/* Close out the standard file descriptors */
        	close(STDIN_FILENO);
        	close(STDOUT_FILENO);
        	close(STDERR_FILENO);
        }
        
        /*
         * Starting up the daemon.
         */
        
		for (i = 0; i < SESSIONBUCKETS; i++){ // Initialize all the slots in the hashtable to NULL.
			sessiontable[i].next = NULL;
			sessiontable[i].prev = NULL;
		}
        
        if (numworkers == 0){
      		numworkers = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.
      		numworkers = numworkers * 2;
        }
      	
      	for (i = 0; i < numworkers; i++){
			pthread_create(&workers[i].t_worker, NULL, worker_function, &workers[i]);
			pthread_cond_init(&workers[i].signal, NULL); // Initialize the thread signal.
			workers[i].queue.next = NULL; // Initialize the queue.
			workers[i].queue.prev = NULL;
			workers[i].lzbuffer = NULL;
			workers[i].lzfastbuffer = NULL;
			workers[i].sessions = 0;
			workers[i].queue.qlen = 0;
			pthread_mutex_init(&workers[i].queue.lock, NULL); // Initialize the queue lock.
			workers[i].state = RUNNING;
			pthread_mutex_init(&workers[i].lock, NULL); // Initialize the worker lock.
      	}

        /*
         * Create the fetcher thread that retrieves
         * IP packets from the Netfilter Queue.
         */
		pthread_create(&t_fetcher, NULL, fetcher_function, NULL);
		pthread_create(&t_cleanup, NULL, cleanup_function, NULL);
		pthread_create(&t_healthagent, NULL, healthagent_function, NULL);
		
		/*
		 * Rejoin all threads before we exit!
		 */
        pthread_join(t_fetcher, NULL);
        pthread_join(t_cleanup, NULL);
        pthread_join(t_healthagent, NULL);
        
        for (i = 0; i < numworkers; i++){
        	
        	workers[i].state = STOPPED;
        	pthread_mutex_lock(&workers[i].queue.lock);
        	pthread_cond_signal(&workers[i].signal);
        	pthread_mutex_unlock(&workers[i].queue.lock);
        	pthread_join(workers[i].t_worker, NULL);
        }
        
        /*
         * Free any allocated resources before exiting
         */
        close(rawsock); // close the socket used for sending keepalives.
        rawsock = 0;
        
        for (i = 0; i < SESSIONBUCKETS; i++){ // Initialize all the slots in the hashtable to NULL.
        	if (sessiontable[i].next != NULL){
				freemem(&sessiontable[i]);
				sprintf(message, "Exiting: Freeing sessiontable %d!\n",i);
				logger(message);
        	}
			
		}
		
        sprintf(message, "Exiting: %s daemon exiting", DAEMON_NAME);
		logger(message);
        
   exit(EXIT_SUCCESS);
}
