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

#include <linux/types.h>

#include <sys/socket.h>
#include <sys/unistd.h>

#include "wccpv2.h"
#include "clicommands.h"
#include "logger.h"
#include "list.h"

/**
 * Do be honest I am not sure what ones of these I am actually going to need
 * but I will define everything here as described in the RFC draft.
 */

#define WCCP_PASSWORD_LENGTH					8

// Section 4.1
#define HERE_I_AM_T								10

// Section 5
#define WCCP_PORT								2048

// Section 5.5
#define WCCP2_HERE_I_AM							10
#define WCCP2_I_SEE_YOU							11
#define WCCP2_REDIRECT_ASSIGN					12
#define WCCP2_REMOVAL_QUERY						13
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
#define WCCP2_CAPABILTIY_INFO					8

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
#define	WCCP2_FORWARDING_METHOD					0x01
#define WCCP2_ASSIGNMENT_METHOD					0x02
#define WCCP2_PACKET_RETURN_METHOD				0x03

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

// Section 5.6.3
struct wccp_router_id_info{
	__u16 type;
	__u16 length;
	__u32 router_id;
	__u32 send_to;
	__u32 num_received_from;
	// List of __u32 IP addresses 0-n.
};

// Section 5.6.4
struct wccp_webcache_id_info{
	__u16 type;
	__u16 length;
	// Web Cache Identity Element 5.7.2
};

struct wccp_service_group{
	struct list_item groups; // Links to other service groups.
	__u8 group_id;
	struct list_head servers; // List of WCCP servers in this group.
	char password[WCCP_PASSWORD_LENGTH]; // Limited to 8 characters.

};

struct wccp_server{
	struct list_item servers; // Links to other wccp servers in this group.
	__u32 ipaddress; // IP address of this server.
	int sock; // fd used to communicate with this wccp server.
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

void *wccp_thread(void *dummyPtr) {
	char message[LOGSZ];

	sprintf(message, "Starting WCCP Thread.\n");
	logger2(LOGGING_DEBUG, DEBUG_WCCP,message);


	sprintf(message, "Exiting WCCP Thread.\n");
	logger2(LOGGING_DEBUG, DEBUG_WCCP,message);
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




