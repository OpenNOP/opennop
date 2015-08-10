/**
 * http://www.web-cache.com/Writings/Internet-Drafts/draft-wilson-wrec-wccp-v2-00.txt
 * http://www.opennet.ru/base/net/wccp2_squid.txt.html
 * http://www.ciscopress.com/articles/article.asp?p=1192686&seqNum=2
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h> // for multi-threading
#include <ctype.h>

#include <arpa/inet.h>

#include <linux/types.h>

#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/time.h>

#include "sockets.h"
#include "wccpv2.h"
#include "clicommands.h"
#include "logger.h"
#include "list.h"
#include "opennopd.h"
#include "utility.h"

/**
 * Do be honest I am not sure what ones of these I am actually going to need
 * but I will define everything here as described in the RFC draft.
 */

#define WCCP_PASSWORD_LENGTH					8

// Section 4.1
#define HERE_I_AM_T								10000

// Section 5
#define WCCP_PORT								2048

// Section 5.5
typedef enum {
	WCCP2_HERE_I_AM			= 10,
	WCCP2_I_SEE_YOU			= 11,
	WCCP2_REDIRECT_ASSIGN	= 12,
	WCCP2_REMOVAL_QUERY		= 13
} WCCP2_MSG_TYPE;

#define WCCP2_VERSION							0x200

// Section 5.6.1
#define WCCP2_SECURITY_INFO						0
#define WCCP2_NO_SECURITY						0
#define WCCP2_MD5_SECURITY						1



// Section 5.6.2
#define WCCP2_SERVICE_INFO						1
#define WCCP2_SERVICE_STANDARD					0
#define WCCP2_SERVICE_DYNAMIC					1
// Well known services
#define WCCP2_SERVICE_ID_HTTP					0x00
// Service Flags
#define WCCP2_SERVICE_FLAG_SOURCE_IP_HASH		0x0001
#define WCCP2_SERVICE_FLAG_DEST_IP_HASH			0x0002
#define WCCP2_SERVICE_FLAG_SOURCE_PORT_HASH		0x0004
#define WCCP2_SERVICE_FLAG_DEST_PORT_HASH		0x0008
#define WCCP2_SERVICE_FLAG_PORTS_DEFINED		0x0010
#define WCCP2_SERVICE_FLAG_PORTS_SOURCE			0x0020
#define WCCP2_SERVICE_FLAG_SOURCE_IP_ALT_HASH	0x0100
#define WCCP2_SERVICE_FLAG_DEST_IP_ALT_HASH		0x0200
#define WCCP2_SERVICE_FLAG_SOURCE_PORT_ALT_HASH	0x0400
#define WCCP2_SERVICE_FLAG_DEST_PORT_ALT_HASH	0x0800

// Section 5.6.3
#define WCCP2_ROUTER_ID_INFO					2

// Section 5.6.4
#define WCCP2_WC_ID_INFO						3

// Section 5.6.5
#define WCCP2_RTR_VIEW_INFO						4

// Section 5.6.6
#define WCCP2_WC_VIEW_INFO						5

// Section 5.6.7
#define WCCP2_REDIRECT_ASSIGNMENT				6

// Section 5.6.8
#define WCCP2_QUERY_INFO						7

// Section 5.6.9 (optional)
#define WCCP2_CAPABILITY_INFO					8

// Section 5.6.10
#define WCCP2_ALT_ASSIGNMENT					13
#define WCCP2_HASH_ASSIGNMENT					0x00
#define WCCP2_MASK_ASSIGNMENT					0x01

// Section 5.6.11
#define WCCP2_ASSIGN_MAP 						14

// Section 5.6.12 (optional)
#define WCCP2_COMMAND_EXTENSION 				15
// Command types
#define WCCP2_COMMAND_TYPE_SHUTDOWN 			01
#define WCCP2_COMMAND_TYPE_SHUTDOWN_RESPONSE	02

// Section 5.7.4
//#define	WCCP2_FORWARDING_METHOD					0x01
//#define WCCP2_ASSIGNMENT_METHOD					0x02
//#define WCCP2_PACKET_RETURN_METHOD				0x03

typedef enum {
	WCCP2_FORWARDING_METHOD = 0x01,
	WCCP2_ASSIGNMENT_METHOD = 0x02,
	WCCP2_PACKET_RETURN_METHOD = 0x03
} WCCP2_CAPABILITY_TYPE;

// Section 5.7.5
#define	WCCP2_FORWARDING_METHOD_GRE     		0x00000001
#define	WCCP2_FORWARDING_METHOD_L2      		0x00000002
#define	WCCP2_ASSIGNMENT_METHOD_HASH    		0x00000001
#define	WCCP2_ASSIGNEMNT_METHOD_MASK    		0x00000002
#define	WCCP2_PACKET_RETURN_METHOD_GRE  		0x00000001
#define	WCCP2_PACKET_RETURN_METHOD_L2   		0x00000002

// Section 5.5
struct wccp2_message_header {
    __u32 type;
    __u16 version;
    __u16 length;
};

// Section 5.6.1
struct wccp_security_info {
	__u16 type;
	__u16 length;
	__u32 security_option;
};

// Section 5.6.2
struct wccp_service_info {
	__u16 type;
	__u16 length;
	__u8  service_type;
	__u8  service_id;
	__u8  priority;
	__u8  protocol;
	__u32  service_flags;
	__u8  ports[8];
};

struct wccp_router_id_element{
	__u32 router_id;
	__u32 receive_id;
};

// Section 5.6.3
struct wccp_router_id_info{
	__u16 type;
	__u16 length;
	__u32 router_ip;
	__u32 router_id;
	__u32 send_to;
	__u32 num_received_from;
	// List of __u32 IP addresses 0-n.
};

// Section 5.6.4
struct wccp_webcache_id_info{
	__u16 type;
	__u16 length;
	__u32 cache_ip;
	__u16 hash_revision;
	__u16 reserved;
	__u32 bucket[8];
	__u16 weight;
	__u16 status;

	// Web Cache Identity Element 5.7.2
};

struct wccp_webcache_view_info{
	__u16 type;
	__u16 length;
	__u32 change_number;
	/* __u32 number_of_routers */
	/* [n]__u32 router_id */
	/* __u32 number_of_webcaches */
	/* [n]__u32 webcache_address */
};

struct wccp_capability_info{
	__u16 type;
	__u16 length;
};

struct wccp_capability_element{
	__u16 type;
	__u16 length;
	__u32 value;
};



struct wccp_service_group{
	struct list_item groups; // Links to other service groups.
	__u8 group_id;
	__u32 change_number;
	struct list_head servers; // List of WCCP servers in this group.
	char password[WCCP_PASSWORD_LENGTH]; // Limited to 8 characters.

};

struct wccp_server{
	struct list_item servers; // Links to other wccp servers in this group.
	__u32 ipaddress; // IP address of this server.
	__u32 router_id;
	int sock; // fd used to communicate with this wccp server.
	time_t hellotimer; // Last hello message send or attempted.
};

static pthread_t t_wccp; // thread for wccp.
static struct list_head wccp_service_groups;
static struct command_head cli_wccp_mode; // list of wccp commands.
static int DEBUG_WCCP = LOGGING_DEBUG;


int cli_enter_wccp_help(int client_fd) {
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Usage: wccp <service group (0-254)>\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

/** @brief Test a string if its all numeric.
 *
 * Tests each member of a string to make sure its all digits.
 *
 * @string [in] Pointer to a string array.
 * @todo This should probably be part of the CLI module so it can be used to verify user input.
 */
int isstringdigits(char *string){
	int i;

	for(i=0;i < strlen(string);i++){

		if(isdigit(string[i]) == 0){
			return false;
		}
	}
	return true;
}

struct wccp_service_group *allocate_wccp_service_group(__u8 servicegroup){
	struct wccp_service_group *new_wccp_service_group = NULL;
	new_wccp_service_group = (struct wccp_service_group*) malloc(sizeof(struct wccp_service_group));

	if(new_wccp_service_group != NULL){
		new_wccp_service_group->change_number = 0;
		new_wccp_service_group->groups.next = NULL;
		new_wccp_service_group->groups.prev = NULL;
		new_wccp_service_group->group_id = servicegroup;
		new_wccp_service_group->servers.next = NULL;
		new_wccp_service_group->servers.prev = NULL;
		new_wccp_service_group->servers.count = 0;
		pthread_mutex_init(&new_wccp_service_group->servers.lock, NULL);
	}

	return new_wccp_service_group;
}

struct wccp_service_group *find_wccp_service_group(__u8 servicegroup){

	struct wccp_service_group *current_wccp_service_group = wccp_service_groups.next;

	while(current_wccp_service_group != NULL){

		if(current_wccp_service_group->group_id == servicegroup){
			return current_wccp_service_group;
		}
		current_wccp_service_group = current_wccp_service_group->groups.next;
	}

	return NULL;
}


/** @brief Put CLI into WCCP configuration mode.
 *
 * Puts the CLI into WCCP configuration mode if a valid service group was provided.
 *
 * @client_fd [in] The CLI socket that send the command.
 * @param parameters [in] Should be single integer 0-254.
 * @param numparameters [in] Should be 1.
 */
struct commandresult cli_enter_wccp_mode(int client_fd, char **parameters, int numparameters, void *data) {
	char msg[MAX_BUFFER_SIZE] = { 0 };
	struct commandresult result = { 0 };
	struct wccp_service_group *current_wccp_service_group = NULL;
	int temp = 0;
	__u8 servicegroup = 0;

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    /*
     *
     */
    if(numparameters == 1){

    	if(isstringdigits(parameters[0]) == true){

    		temp = atoi(parameters[0]);

    		if((temp >= 0) && (temp <= 254)){
    			servicegroup = (__u8)temp;
    			/*
    			 * @todo Should look for the WCCP group structure.
    			 * If it exists set result.data to this structure.
    			 * If it does not exist we need to create it and set result.data to this structure.
    			 */
    			current_wccp_service_group = find_wccp_service_group(servicegroup);

    			if(current_wccp_service_group == NULL){
    			    sprintf(msg,"No service group.\n");
    			    cli_send_feedback(client_fd, msg);

    			    current_wccp_service_group = allocate_wccp_service_group(servicegroup);
    			    insert_list_item(&wccp_service_groups, current_wccp_service_group);
    			}

    			result.mode = &cli_wccp_mode;
    			result.data = current_wccp_service_group;
    		}else{
    			cli_enter_wccp_help(client_fd);
    		}

    	}else{
    		cli_enter_wccp_help(client_fd);
    	}
    }else{
    	cli_enter_wccp_help(client_fd);
    }

    return result;
}

struct commandresult cli_exit_wccp_mode(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

struct wccp_server *find_wccp_server(struct wccp_service_group *service_group, __u32 serverip){

	struct wccp_server *current_wccp_server = service_group->servers.next;

	while(current_wccp_server != NULL){

		if(current_wccp_server->ipaddress == serverip){
			return current_wccp_server;
		}
		current_wccp_server = current_wccp_server->servers.next;
	}

	return NULL;
}

struct wccp_server *allocate_wccp_server(__u32 serverip){
	struct wccp_server *new_wccp_server = NULL;
	new_wccp_server = (struct wccp_server*) malloc(sizeof(struct wccp_server));

	if(new_wccp_server != NULL){
		new_wccp_server->servers.head = NULL;
		new_wccp_server->servers.next = NULL;
		new_wccp_server->servers.prev = NULL;
		new_wccp_server->ipaddress = serverip;
		new_wccp_server->router_id = 0;
		new_wccp_server->sock = 0;
		time(&new_wccp_server->hellotimer);
	}

	return new_wccp_server;
}

int cli_wccp_server_help(int client_fd) {
    char msg[MAX_BUFFER_SIZE] = { 0 };

    sprintf(msg,"Usage: server <ip address>\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

struct commandresult cli_wccp_server(int client_fd, char **parameters, int numparameters, void *data) {
	char msg[MAX_BUFFER_SIZE] = { 0 };
	struct commandresult result = { 0 };
	int ERROR = 0;
	__u32 serverIP = 0;
	struct wccp_server *current_wccp_server = NULL;


	result.finished = 0;
    result.mode = &cli_wccp_mode;
    result.data = NULL;

    /*
     * There must be data passed from the cli mode.
     * This should point to the WCCP Service Group data structure.
     * If this data is not passed in by the wccp mode we need to exit.
     */
    if (data != NULL){
    	result.data = data;
    }else{
    	result.mode = NULL;
    	return result;
    }

    /*
     * First check for the correct number of parameters.
     */
    if(numparameters == 1){

    	/*
    	 * Next we check to make sure its a valid IP.
    	 * We dont need to check if its actually a WCCP server
    	 * just assume the admin knows what they are doing.
    	 */
        ERROR = inet_pton(AF_INET, parameters[0], &serverIP); //Should return 1.

        if(ERROR != 1) {
            cli_wccp_server_help(client_fd);
            return result;
        }

        current_wccp_server = find_wccp_server((struct wccp_service_group *)data, serverIP);

        if(current_wccp_server == NULL){
        	 sprintf(msg,"No server found.\n");
        	 cli_send_feedback(client_fd, msg);

        	 current_wccp_server = allocate_wccp_server(serverIP);
        	 insert_list_item(&((struct wccp_service_group *)data)->servers, current_wccp_server);
        }

    }else{
    	cli_wccp_server_help(client_fd);
    }

    return result;
}

struct commandresult cli_wccp_password(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };

    result.finished = 0;
    result.mode = &cli_wccp_mode;
    result.data = NULL;

    return result;
}

int get_wccp_message_length(struct wccp2_message_header *wccp2_msg_header){

	return (ntohs(wccp2_msg_header->length) + 8);
}

int update_wccp_message_length(struct wccp2_message_header *wccp2_msg_header, __u16 length){
	wccp2_msg_header->length = htons(ntohs(wccp2_msg_header->length) + ntohs(length) + 4);

	return 0;
}

int wccp_add_here_i_am_header(struct wccp2_message_header *wccp2_msg_header){
	wccp2_msg_header->type = htonl(WCCP2_HERE_I_AM);
	wccp2_msg_header->version = htons(WCCP2_VERSION);
	wccp2_msg_header->length = htons(0);
	return 0;
}

int wccp_add_security_component(struct wccp2_message_header *wccp2_msg_header){
	struct wccp_security_info *wccp2_security_component;

	wccp2_security_component = (char *)wccp2_msg_header + get_wccp_message_length(wccp2_msg_header);

	wccp2_security_component->type = htons(WCCP2_SECURITY_INFO);
	wccp2_security_component->length = htons(4);
	wccp2_security_component->security_option = htonl(WCCP2_NO_SECURITY);

	update_wccp_message_length(wccp2_msg_header, wccp2_security_component->length);

	return 0;
}

int wccp_add_service_component(struct wccp_service_group *this_wccp_service_group, struct wccp2_message_header *wccp2_msg_header){
	struct wccp_service_info *wccp2_service_component;
	wccp2_service_component = (char *)wccp2_msg_header + get_wccp_message_length(wccp2_msg_header);

	wccp2_service_component->type = htons(WCCP2_SERVICE_INFO);
	wccp2_service_component->length = htons(24);
	wccp2_service_component->service_type = WCCP2_SERVICE_DYNAMIC ;
	wccp2_service_component->service_id = htons(this_wccp_service_group->group_id);
	wccp2_service_component->priority = 200;
	wccp2_service_component->protocol = 0;
	wccp2_service_component->service_flags = htonl(15);

	update_wccp_message_length(wccp2_msg_header, wccp2_service_component->length);

	return 0;
}

int wccp_add_router_id_component(struct wccp2_message_header *wccp2_msg_header, __u32 routerip){
	struct wccp_router_id_info *wccp2_router_id_component;
	wccp2_router_id_component = (char *)wccp2_msg_header + get_wccp_message_length(wccp2_msg_header);

	wccp2_router_id_component->type = htons(WCCP2_ROUTER_ID_INFO);
	wccp2_router_id_component->length = htons(16);
	wccp2_router_id_component->router_ip = routerip;
	wccp2_router_id_component->send_to = 0;

	update_wccp_message_length(wccp2_msg_header, wccp2_router_id_component->length);

	return 0;
}

int wccp_add_webcache_id_component(struct wccp2_message_header *wccp2_msg_header){
	struct wccp_webcache_id_info *wccp2_webcache_id_component;
	wccp2_webcache_id_component = (char *)wccp2_msg_header + get_wccp_message_length(wccp2_msg_header);

	wccp2_webcache_id_component->type = htons(WCCP2_WC_ID_INFO);
	wccp2_webcache_id_component->length = htons(44);
	wccp2_webcache_id_component->cache_ip = get_local_ip();
	wccp2_webcache_id_component->hash_revision = htons(0x00);
	wccp2_webcache_id_component->weight = 0;
	wccp2_webcache_id_component->status = 0;

	update_wccp_message_length(wccp2_msg_header, wccp2_webcache_id_component->length);

	return 0;
}

int wccp_add_webcache_view_component(struct wccp_service_group *this_wccp_service_group, struct wccp2_message_header *wccp2_msg_header){
	struct wccp_webcache_view_info *wccp2_webcache_view_component;
	struct wccp_server *this_wccp_server = NULL;
	struct wccp_router_id_element *router_id = NULL;
	__u32 *num_routers = NULL;
	__u32 *num_webcaches = NULL;
	char message[LOGSZ] = {0};
	wccp2_webcache_view_component = (char *)wccp2_msg_header + get_wccp_message_length(wccp2_msg_header);

	wccp2_webcache_view_component->type = htons(WCCP2_WC_VIEW_INFO);
	wccp2_webcache_view_component->length = htons(4);
	wccp2_webcache_view_component->change_number = htonl(this_wccp_service_group->change_number);

	num_routers = (char*)wccp2_webcache_view_component + 8;
	sprintf(message,"[WCCP] Number of Routers: %u.\n", this_wccp_service_group->servers.count);
	logger2(LOGGING_DEBUG, DEBUG_WCCP, message);

	*num_routers = htonl(this_wccp_service_group->servers.count);
	wccp2_webcache_view_component->length = htons(ntohs(wccp2_webcache_view_component->length) + 4);

	this_wccp_server = this_wccp_service_group->servers.next;
	while(this_wccp_server != NULL){
		router_id = (char*)num_routers + 4;

		router_id->router_id = this_wccp_server->ipaddress;
		router_id->receive_id = htonl(this_wccp_server->router_id);
		wccp2_webcache_view_component->length = htons(ntohs(wccp2_webcache_view_component->length) + sizeof(struct wccp_router_id_element));

		router_id = router_id + 1;
		this_wccp_server = this_wccp_server->servers.next;
	}
	num_webcaches = (char*)router_id + sizeof(struct wccp_router_id_element);

	*num_webcaches = 0;
	wccp2_webcache_view_component->length = htons(ntohs(wccp2_webcache_view_component->length) + 4);

	update_wccp_message_length(wccp2_msg_header, wccp2_webcache_view_component->length);

	return 0;
}

int wccp_add_capability_element(struct wccp_capability_info *wccp_capability_info_component, WCCP2_CAPABILITY_TYPE capability_type, __u32 value){
	struct wccp_capability_element *this_wccp_capability = NULL;
	this_wccp_capability = (char*)wccp_capability_info_component + ntohs(wccp_capability_info_component->length) + 4;
	this_wccp_capability->type = htons(capability_type);
	this_wccp_capability->length = htons(4);
	this_wccp_capability->value = htonl(value);
	wccp_capability_info_component->length = htons(ntohs(wccp_capability_info_component->length) + 8);

	return 0;
}

int wccp_add_capability_info(struct wccp2_message_header *wccp2_msg_header){
	struct wccp_capability_info *wccp_capability_info_component = NULL;

	wccp_capability_info_component = (char *)wccp2_msg_header + get_wccp_message_length(wccp2_msg_header);
	wccp_capability_info_component->type = htons(WCCP2_CAPABILITY_INFO);
	wccp_capability_info_component->length = htons(0);

	wccp_add_capability_element(wccp_capability_info_component,WCCP2_FORWARDING_METHOD, WCCP2_FORWARDING_METHOD_L2);
	wccp_add_capability_element(wccp_capability_info_component,WCCP2_PACKET_RETURN_METHOD, WCCP2_PACKET_RETURN_METHOD_L2);

	update_wccp_message_length(wccp2_msg_header, wccp_capability_info_component->length);

	return 0;
}

int wccp_send_message(struct wccp_service_group *this_wccp_service_group, struct wccp_server *this_wccp_server, WCCP2_MSG_TYPE messagetype){
	char buf[1024] = {0};
	struct wccp2_message_header *wccp2_msg_header;

	wccp2_msg_header = (struct wccp2_message_header *)&buf;

    switch(messagetype) {
    case WCCP2_HERE_I_AM:

    	/*
    	 * 5.1 'Here I Am' Message
    	 *    +--------------------------------------+
    	 *    |         WCCP Message Header          |
    	 *    +--------------------------------------+
    	 *    |       Security Info Component        |
    	 *    +--------------------------------------+
    	 *    |        Service Info Component        |
    	 *    +--------------------------------------+
    	 *    |  Web-Cache Identity Info Component   |
    	 *    +--------------------------------------+
    	 *    |    Web-Cache View Info Component     |
    	 *    +--------------------------------------+
    	 */
    	wccp_add_here_i_am_header(wccp2_msg_header);
    	wccp_add_security_component(wccp2_msg_header);
    	wccp_add_service_component(this_wccp_service_group, wccp2_msg_header);
    	wccp_add_webcache_id_component(wccp2_msg_header);
    	wccp_add_webcache_view_component(this_wccp_service_group, wccp2_msg_header);
    	wccp_add_capability_info(wccp2_msg_header);

    	send(this_wccp_server->sock, wccp2_msg_header, ntohs(wccp2_msg_header->length) + 8, MSG_NOSIGNAL);
        break;
    default:
        logger2(LOGGING_DEBUG,DEBUG_WCCP,"[WCCP] Cannot send unknown type!\n");
    }

	return 0;
}

int process_wccp_router_id_component(int fd, struct wccp_service_group *this_wccp_service_group, struct wccp_router_id_info *wccp2_router_id_component){
	struct wccp_server *this_wccp_server = NULL;
	char message[LOGSZ] = {0};
	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Entering process_wccp_router_id_component().\n");

	this_wccp_server = find_wccp_server(this_wccp_service_group, wccp2_router_id_component->router_ip);

	if(this_wccp_server != NULL){
		this_wccp_server->router_id = ntohl(wccp2_router_id_component->router_id);

		//binary_dump("[WCCP] Router ID", wccp2_router_id_component, ntohs(wccp2_router_id_component->length) + 4);

		//sprintf(message,"[WCCP] Component type: %u in process_wccp_router_id_component().\n", ntohs(wccp2_router_id_component->type));
		//logger2(LOGGING_DEBUG, DEBUG_WCCP, message);

		//sprintf(message,"[WCCP] Router IP %u in process_wccp_router_id_component().\n", ntohl(wccp2_router_id_component->router_ip));
		//logger2(LOGGING_DEBUG, DEBUG_WCCP, message);

		//sprintf(message,"[WCCP] Router ID %u in process_wccp_router_id_component().\n", ntohl(wccp2_router_id_component->router_id));
		//logger2(LOGGING_DEBUG, DEBUG_WCCP, message);
	}

	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Exiting process_wccp_router_id_component().\n");
	return 0;
}

int process_wccp_service_component(int fd, struct wccp_service_info *wccp2_service_component){
	struct wccp_router_id_info *wccp2_router_id_component;
	struct wccp_service_group *this_wccp_service_group = NULL;
	char message[LOGSZ] = {0};
	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Entering process_wccp_service_component().\n");

	this_wccp_service_group = find_wccp_service_group(ntohs(wccp2_service_component->service_id));

	if(this_wccp_service_group != NULL){
		wccp2_router_id_component = (char *)wccp2_service_component + ntohs(wccp2_service_component->length) + 4;
		process_wccp_router_id_component(fd, this_wccp_service_group, wccp2_router_id_component);
	}

	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Exiting process_wccp_service_component().\n");
	return 0;
}

int process_wccp_security_component(int fd, struct wccp_security_info *wccp2_security_component){
	struct wccp_service_info *wccp2_service_component;
	char message[LOGSZ] = {0};
	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Entering process_wccp_security_component().\n");

	wccp2_service_component = (char *)wccp2_security_component + ntohs(wccp2_security_component->length) + 4;

	sprintf(message,"[WCCP] Security option %i in process_wccp_i_see_you().\n", ntohl(wccp2_security_component->security_option));
	logger2(LOGGING_DEBUG, DEBUG_WCCP, message);

	process_wccp_service_component(fd, wccp2_service_component);

	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Exiting process_wccp_security_component().\n");
	return 0;
}

/** @brief Processes WCCP messages sent by WCCP servers.
 *
 * Processes WCCP messages sent by WCCP servers to this client.
 *
 * @param this_epoller [in] poller structure receiving the messages.
 * @param fd [in] socket that the message was received on.
 * @param buf [in] the message.
 * @return int 0 = sucessful 1 = failed
 */
int wccp_handler(struct epoller *this_epoller, int fd, void *buf) {
	struct wccp2_message_header *wccp2_msg_header;
	char message[LOGSZ] = {0};
	wccp2_msg_header = (struct wccp2_message_header *)buf;
	struct wccp_security_info *wccp2_security_component;

	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Entering wccp_handler().\n");

	wccp2_security_component = (char *)wccp2_msg_header + sizeof(struct wccp2_message_header);

	//binary_dump("[WCCP]", buf, 4);

	switch(ntohl(wccp2_msg_header->type)){
		case WCCP2_HERE_I_AM:
			break;
		case WCCP2_I_SEE_YOU:
			logger2(LOGGING_DEBUG, DEBUG_WCCP, "[WCCP] WCCP2_I_SEE_YOU Received.");
			process_wccp_security_component(fd, wccp2_security_component);
			break;
		case WCCP2_REDIRECT_ASSIGN:
			logger2(LOGGING_DEBUG, DEBUG_WCCP, "[WCCP] WCCP2_REDIRECT_ASSIGN Received.");
			break;
		case WCCP2_REMOVAL_QUERY:
			logger2(LOGGING_DEBUG, DEBUG_WCCP, "[WCCP] WCCP2_REMOVAL_QUERY Received.");
			break;
		default:
			sprintf(message,"[WCCP] Unknown message type %i in wccp_handler().\n", ntohl(wccp2_msg_header->type));
			logger2(LOGGING_DEBUG, DEBUG_WCCP, message);
			break;
	}



	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Exiting wccp_handler().\n");
	return 0;
}
/** @brief Processes a WCCP server of a group.
 *
 * Check connection to server.  Open a new one or close if dead.
 * Send WCCP hello messages and login to WCCP group.
 *
 * @param this_epoller [in] wccp server being processed.
 */
int wccp_process_server(struct epoller *wccp_epoller, struct wccp_service_group *this_wccp_service_group, struct wccp_server *this_wccp_server){
	char message[LOGSZ] = {0};
	int client = -1;

	if(this_wccp_server->sock == 0){ //Need to open a socket to this server.
		client = new_udp_client(this_wccp_server->ipaddress, WCCP_PORT);

		if(client >= 0){
			this_wccp_server->sock = client;
			register_socket(client, wccp_epoller->epoll_fd, &wccp_epoller->event);
			logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Connected to server.\n");
		}
	}else{
		logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Sending WCCP Hello to server.\n");
		wccp_send_message(this_wccp_service_group, this_wccp_server, WCCP2_HERE_I_AM);

	}



	return 0;
}

int wccp_epoller_timeout(struct epoller *wccp_epoller){
	time_t currenttime;
	char message[LOGSZ] = {0};
	struct wccp_service_group *current_wccp_service_group = (struct wccp_service_group *)wccp_service_groups.next;
	struct wccp_server *current_wccp_server = NULL;

	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Entering wccp_epoller_timeout().\n");
	time(&currenttime);

	/*
	 * Send a WCCP_HELLO message to each server of each service group.
	 */

	// Loop through each service group.
	while(current_wccp_service_group != NULL){

		current_wccp_server = current_wccp_service_group->servers.next;

		// Send WCCP_HELLO to each server in the group.
		while(current_wccp_server != NULL){

			if((difftime(currenttime, current_wccp_server->hellotimer) >= 9)){
				logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Processing a server.\n");
				time(&current_wccp_server->hellotimer);
				wccp_process_server(wccp_epoller, current_wccp_service_group, current_wccp_server);
			}
			current_wccp_server = current_wccp_server->servers.next;
		}

		current_wccp_service_group = current_wccp_service_group->groups.next;
	}

	logger2(LOGGING_DEBUG, DEBUG_WCCP,"[WCCP] Exiting wccp_epoller_timeout().\n");

	return 0;
}

void *wccp_thread(void *dummyPtr) {
    int error = 0;
    struct epoller wccp_epoller = { 0 };
	char message[LOGSZ];

	logger2(LOGGING_DEBUG, DEBUG_WCCP, "Starting WCCP Thread.\n");

	error = new_ip_epoll_server(&wccp_epoller, NULL, wccp_handler, 0, wccp_epoller_timeout, (HERE_I_AM_T - 1000));

	epoll_handler(&wccp_epoller);

	logger2(LOGGING_DEBUG, DEBUG_WCCP, "Exiting WCCP Thread.\n");

	shutdown_epoll_server(&wccp_epoller);

	return EXIT_SUCCESS;
}

void initialize_wccp_service_groups(){
	wccp_service_groups.next = NULL;
	wccp_service_groups.prev = NULL;
	wccp_service_groups.count = 0;
	pthread_mutex_init(&wccp_service_groups.lock, NULL);
}

void init_wccp(){
	sprintf(cli_wccp_mode.prompt, "opennopd->wccp# ");
	initialize_wccp_service_groups();
    register_command(NULL, "wccp", cli_enter_wccp_mode, true, true);
    register_command(&cli_wccp_mode, "server", cli_wccp_server, true, false);
    register_command(&cli_wccp_mode, "password", cli_wccp_password, true, false);
    register_command(&cli_wccp_mode, "exit", cli_exit_wccp_mode, false, false);
}

void start_wccp(){
	init_wccp();

	pthread_create(&t_wccp, NULL, wccp_thread, (void *) NULL);
}

void stop_wccp(){
	pthread_join(t_wccp, NULL);
}




