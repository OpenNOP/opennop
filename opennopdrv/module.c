#include <linux/module.h> // for all modules
#include <linux/init.h> // for entry/exit macros
#include <linux/kernel.h> // for printk priority macros
#include <linux/ip.h> // for access to IP
#include <linux/tcp.h>
#include <linux/if_ether.h>	// for access to Ethernet
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/timer.h> // to monitor the health of the daemon
#include <linux/version.h> // to test kernel version
#include <linux/string.h>

#include <linux/skbuff.h> // for access to sk_buffs
#include <linux/netfilter_ipv4.h> // for netfilter hooks
#include <linux/netfilter_bridge.h>

#include <net/protocol.h>
#include <net/tcp.h>
#include <net/genetlink.h> // for generic netlink

#define HBINTERVAL 5 /* Number of seconds in heartbeat interval. */

/* Daemon state Definitions */
#define DOWN 0
#define UP 1

/* Bool Definitions */
#define FALSE 0
#define TRUE 1

/* Mode definitions */
#define ROUTED 0
#define BRIDGED 1

/* Global Variables */
int daemonstate = DOWN; /* Current state of the daemon. */
int hbintervals = 3; /* Number of missed heartbeat intervals before Daemon is down. */
struct timeval tv_lasthb; /* Last received "UP" heartbeat from Daemon. */
struct timer_list daemonmonitor; /* Timer that checks the Daemon state. */
int selectedmode = ROUTED;

/* Debug Variables */ 
int DEBUG_HEARTBEAT = FALSE;

#include "opennop_netlink.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Justin Yaple <yaplej@gmail.com>");
MODULE_DESCRIPTION("Intercepts network traffic and queues it to the OpenNOP daemon.");
MODULE_ALIAS("opennopdrv");

static char *mode = "routed";
module_param(mode, charp, S_IRUGO);

static void daemonmonitor_function(unsigned long data){
	struct timeval tv_now; // current time.
	int timespan;
	timespan = hbintervals * HBINTERVAL;
	do_gettimeofday(&tv_now); // Get the time from hardware.
	
	/*
	 *  Check that the daemon has been sending heartbeats. 
	 */
	 if (tv_now.tv_sec - timespan >= tv_lasthb.tv_sec){
	 	daemonstate = DOWN;
	 	
	 	if (DEBUG_HEARTBEAT == TRUE){
	 		printk(KERN_ALERT "No heartbeat. timespan:%i now:%i, lasthb:%i \n",timespan, (unsigned int)tv_now.tv_sec, (unsigned int)tv_lasthb.tv_sec);
	 	}
	 }
	 else{
	 	daemonstate = UP;
	 	
	 	if (DEBUG_HEARTBEAT == TRUE){
	 		printk(KERN_ALERT "Got a heartbeat. timespan:%i now:%i, lasthb:%i \n",timespan, (unsigned int)tv_now.tv_sec, (unsigned int)tv_lasthb.tv_sec);
	 	}
	 }
	 
mod_timer(&daemonmonitor, jiffies + (hbintervals * HBINTERVAL) * HZ); // Update the timer for the next run.
}

/*
 * The structure for netfilter queue functions changed sometime
 * around kernel version 2.6.20 to handle this there are two different
 * declarations of struct sk_buff depending on what kernel version
 * is present on the system.
 */

/* Rewritten for kernel < and >= 2.6.20 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
static unsigned int
	queue_function(unsigned int hooknum, 
	struct sk_buff **skb,
	const struct net_device *in, 
	const struct net_device *out, 
	int (*okfn)(struct sk_buff *)){
#else
static unsigned int
	queue_function(unsigned int hooknum, 
	struct sk_buff *skb,
	const struct net_device *in, 
	const struct net_device *out, 
	int (*okfn)(struct sk_buff *)){
#endif

	struct iphdr *iph = NULL;

	/*
	 * yaplej: Process the packet only,
	 * if it was intended for this host,
	 * is not NULL,
	 * and is an IP packet.
	 *
	 * This limits processing packets to in-bound traffic only!
	 */
	 
	 /* Rewritten for kernel < and >= 2.6.20 */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
		 if ((skb != NULL) && 
		((*skb)->pkt_type == PACKET_HOST) && 
		((*skb)->protocol == htons(ETH_P_IP))){
		iph = ip_hdr((*skb)); // access ip header.
	#else
		if ((skb != NULL) && 
		((skb)->pkt_type == PACKET_HOST) && 
		((skb)->protocol == htons(ETH_P_IP))){
		iph = ip_hdr((skb)); // access ip header.
	#endif

		/*
		 * yaple: Process only TCP segments.
		 */
		
		if (iph->protocol == IPPROTO_TCP){
			
			/*
			 * Check that the daemon is UP.
			 */
			 if (daemonstate == UP) {
			 	return NF_QUEUE;
			 }
		}
	}
	
	return NF_ACCEPT;
}

static struct nf_hook_ops opennop_hook = { // This is a netfilter hook.
	.hook = queue_function, // Function that executes.
	
	/* Rewritten for kernel < and >= 2.6.20 */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
		.hooknum = NF_IP_FORWARD, // For routed traffic only.
	#else
		.hooknum = NF_INET_FORWARD, // For routed ip traffic only.
	#endif
	.pf = PF_INET, // Only for IP packets.
	.priority = NF_IP_PRI_FIRST, // My hook executes first.
};

static int __init opennopdrv_init(void){
	int err, i;
	int timespan;
	size_t n_ops;
	struct genl_ops *ops;
	
	timespan = hbintervals * HBINTERVAL;


#if (__MY_NL_VERSION__ == __MY_NL_NEWEST__)
	printk(KERN_ALERT "[OpenNOPDrv]: Kernel Version >= 3.13.0 \n");
	printk(KERN_ALERT "[OpenNOPDrv]: opennop_nl_ops %lu\n", ARRAY_SIZE(opennop_nl_ops));
	err = genl_register_family_with_ops(&opennop_nl_family, opennop_nl_ops);

	if (err != 0){
		return err;
	}
#elif (__MY_NL_VERSION__ == __MY_NL_NEWER__)
	printk(KERN_ALERT "[OpenNOPDrv]: Kernel Version >= 3.11.0 \n");
	printk(KERN_ALERT "[OpenNOPDrv]: opennop_nl_ops %lu\n", ARRAY_SIZE(opennop_nl_ops));
	err = genl_register_family_with_ops(&opennop_nl_family, opennop_nl_ops, ARRAY_SIZE(opennop_nl_ops));

	if (err != 0){
		return err;
	}
#elif (__MY_NL_VERSION__ == __MY_NL_OLDEST__)
	printk(KERN_ALERT "[OpenNOPDrv]: Kernel Version < 3.0.0 \n");
	err = genl_register_family(&opennop_nl_family);

	if (err != 0){
		return err;
	}

	printk(KERN_ALERT "[OpenNOPDrv]: opennop_nl_ops %lu\n", ARRAY_SIZE(opennop_nl_ops));
	n_ops = ARRAY_SIZE(opennop_nl_ops);
	ops = (struct genl_ops *)opennop_nl_ops;

	for (i = 0; i < n_ops; ++i, ++ops) {
		err = genl_register_ops(&opennop_nl_family, ops);

		if (err != 0){
			return err;
		}
	}
#else
	printk(KERN_ALERT "[OpenNOPDrv]: Kernel Version ERROR.\n");
	return -1;
#endif

	if (strcmp(mode, "bridged") == 0) {
		//printk(KERN_ALERT "[OpenNOPDrv]: Switching to bridged mode. \n");
		/* Rewritten for kernel < and >= 2.6.30 */
		#if (LINUX_VERSION_CODE > KERNEL_VERSION (2, 6, 30))
			opennop_hook.hooknum = NF_BR_FORWARD; // For bridged traffic only.
		#else
			printk(KERN_ALERT "[OpenNOPDrv]: Bridged mode only supported with kernel > 2.6.30 \n");
		#endif

	}
	printk(KERN_ALERT "[OpenNOPDrv]: %s \n", mode);
	nf_register_hook(&opennop_hook);
	init_timer(&daemonmonitor);
	daemonmonitor.expires = jiffies + (timespan * HZ);
	daemonmonitor.data = 0;
	daemonmonitor.function = daemonmonitor_function;
	add_timer(&daemonmonitor);
	return 0;
} 

static void __exit opennopdrv_exit(void){
	genl_unregister_family(&opennop_nl_family);
	nf_unregister_hook(&opennop_hook);
	del_timer_sync(&daemonmonitor);
}

module_init(opennopdrv_init);
module_exit(opennopdrv_exit);
