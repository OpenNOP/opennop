/* attributes (variables): the index in this enum is used as a reference for the type,
 *             userspace application has to indicate the corresponding type
 *             the policy is used for security considerations 
 */
enum {
	OPENNOP_A_UNSPEC,
	OPENNOP_A_MSG,
        __OPENNOP_A_MAX,
};
#define OPENNOP_A_MAX (__OPENNOP_A_MAX - 1)

/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h 
 */
static struct nla_policy opennop_nl_policy[OPENNOP_A_MAX + 1] = {
	[OPENNOP_A_MSG] = { .type = NLA_NUL_STRING },
};

#define VERSION_NR 1
/* family definition */
static struct genl_family opennop_nl_family = {
	.id = GENL_ID_GENERATE,         //genetlink should generate an id
	.hdrsize = 0,
	.name = "OPENNOP",        //the name of this family, used by userspace application
	.version = VERSION_NR,                   //version number  
	.maxattr = OPENNOP_A_MAX,
};

/* commands: enumeration of all commands (functions), 
 * used by userspace application to identify command to be ececuted
 */
enum {
	OPENNOP_CMD_NOOP,
	OPENNOP_CMD_ECHO,
	OPENNOP_CMD_HB,
	__OPENNOP_CMD_MAX,
};
#define OPENNOP_CMD_MAX (__OPENNOP_CMD_MAX - 1)

static int opennop_nl_cmd_noop(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	void *hdr;
	int ret = -ENOBUFS;

	#if (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
		msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	#else
		msg = genlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	#endif

	if (!msg) {
		ret = -ENOMEM;
		goto out;
	}

	#if (LINUX_VERSION_CODE > KERNEL_VERSION (3, 6, 11))
		hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq, &opennop_nl_family, 0, OPENNOP_CMD_NOOP);
	#elif (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
		hdr = genlmsg_put(msg, info->snd_pid, info->snd_seq, opennop_nl_family.id, 0, 0, OPENNOP_CMD_NOOP, opennop_nl_family.version);
	#else
		hdr = genlmsg_put(msg, info->snd_pid, info->snd_seq, &opennop_nl_family, 0, OPENNOP_CMD_NOOP);
	#endif



	if (!hdr) {
		ret = -EMSGSIZE;
		goto err_out;
	}

	genlmsg_end(msg, hdr);

#if (LINUX_VERSION_CODE > KERNEL_VERSION (3, 6, 11))
	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
	return genlmsg_unicast(msg, info->snd_pid);
#else
	return genlmsg_unicast(genl_info_net(info), msg, info->snd_pid);
#endif

err_out:
	nlmsg_free(msg);

out:
	return ret;
}

/* an echo command, receives a message, prints it and sends another message back */
int opennop_nl_cmd_echo(struct sk_buff *skb_2, struct genl_info *info)
{
	struct nlattr *na;
	struct sk_buff *skb;
	int rc;
	void *msg_head;
	char * mydata;
	
        if (info == NULL)
                goto out;
  
        /*for each attribute there is an index in info->attrs which points to a nlattr structure
         *in this structure the data is given
         */
        na = info->attrs[OPENNOP_A_MSG];
       	if (na) {
		mydata = (char *)nla_data(na);
		if (mydata == NULL)
			printk("error while receiving data\n");
		else
			printk("received: %s\n", mydata);
		}
	else
		printk("no info->attrs %i\n", OPENNOP_A_MSG);

        /* send a message back*/
        /* allocate some memory, since the size is not yet known use NLMSG_GOODSIZE*/	
        
        /*
         * Newer method for kernel 2.6.20 and greater.
         *  skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
         */
         
		/* Rewritten for kernel < and >= 2.6.20 */
		#if (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
			skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
		#else
			skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
		#endif
         
	if (skb == NULL)
		goto out;

	/* create the message headers */
        /* arguments of genlmsg_put: 
           struct sk_buff *, 
           int (sending) pid, 
           int sequence number, 
           struct genl_family *, 
           int flags, 
           u8 command index (why do we need this?)
        */
        /*
         * For kernel 2.6.20 or greater.
         * msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &doc_exmpl_gnl_family, 0, DOC_EXMPL_C_ECHO); 
         */
         
		/* Rewritten for kernel < and >= 2.6.20 */
		#if (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
			msg_head = genlmsg_put(skb, 0, info->snd_seq+1, opennop_nl_family.id, 0, 0, OPENNOP_CMD_ECHO, opennop_nl_family.version);
 		#else
 			msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &opennop_nl_family, 0, OPENNOP_CMD_ECHO);
		#endif
 
	if (msg_head == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	/* add a DOC_EXMPL_A_MSG attribute (actual value to be sent) */
	rc = nla_put_string(skb, OPENNOP_A_MSG, "hello world from kernel space");
	if (rc != 0){
		goto out;
	}
		/* finalize the message */
		genlmsg_end(skb, msg_head);

        /* send the message back */
		/* Rewritten for kernel < and >= 2.6.20 */
		#if (LINUX_VERSION_CODE > KERNEL_VERSION (3, 6, 11))
			rc = genlmsg_unicast(NULL,skb,info->snd_portid );
		#elif (LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 20))
			rc = genlmsg_unicast(skb,info->snd_pid );
		#else
			rc = genlmsg_unicast(NULL,skb,info->snd_pid );
		#endif
			
	if (rc != 0){
		goto out;
	}
	return 0;

 out:
        printk("an error occured in doc_exmpl_echo:\n");
  
      return 0;
}


/* an heartbeat command, receives a message, prints it and sends another message back */
int opennop_nl_cmd_hb(struct sk_buff *skb_2, struct genl_info *info)
{
	struct nlattr *na;
	struct timeval tv_now; // current time.
	//struct sk_buff *skb;
	//int rc;
	//void *msg_head;
	char * mydata;
	
	if (info == NULL){
		goto out;
	}
	/*for each attribute there is an index in info->attrs which points to a nlattr structure
	*in this structure the data is given
	*/
	na = info->attrs[OPENNOP_A_MSG];
	if (na) {
		mydata = (char *)nla_data(na);
		if (mydata == NULL){
	
			if (DEBUG_HEARTBEAT == TRUE){
				printk("error while receiving data\n");
			}
		}	
		else{
	
			if (DEBUG_HEARTBEAT == TRUE){
				printk("received: %s\n", mydata);
			}
		
			if (strcmp(mydata,"UP") == 0){
			
				if (DEBUG_HEARTBEAT == TRUE){
					printk(KERN_ALERT "Service is running.\n");
				}
				do_gettimeofday(&tv_now); // Get the time from hardware.
				tv_lasthb = tv_now; // Update the global heartbeat variable.
				daemonstate = UP; // Update the Daemon state. 
			}
			else if (strcmp(mydata,"DOWN") == 0){
			
				if (DEBUG_HEARTBEAT == TRUE){
					printk(KERN_ALERT "Service is down.\n");
				}
				daemonstate = DOWN; // Update the Daemon state.
			}
		}
	}
	else{
		
		if (DEBUG_HEARTBEAT == TRUE){	
			printk(KERN_ALERT "no info->attrs %i\n", OPENNOP_A_MSG);
		}
	}
	return 0;

 	out:
 	
 		if (DEBUG_HEARTBEAT == TRUE){
        	printk(KERN_ALERT "an error occured in opennop_hb:\n");
 		}
  
      return 0;
}


/* commands: mapping between the command enumeration and the actual function*/
static const struct genl_ops opennop_nl_ops[] = {
	{
			.cmd = OPENNOP_CMD_NOOP,
			.doit = opennop_nl_cmd_noop,
			.policy = opennop_nl_policy,
			/* can be retrieved by unprivileged users */
	},
	{
			.cmd = OPENNOP_CMD_ECHO,
			.flags = 0,
			.policy = opennop_nl_policy,
			.doit = opennop_nl_cmd_echo,
			.dumpit = NULL,
	},
	{
			.cmd = OPENNOP_CMD_HB,
			.flags = 0,
			.policy = opennop_nl_policy,
			.doit = opennop_nl_cmd_hb,
			.dumpit = NULL,
	},
};

