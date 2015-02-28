/**
 * http://www.web-cache.com/Writings/Internet-Drafts/draft-wilson-wrec-wccp-v2-00.txt
 * http://www.opennet.ru/base/net/wccp2_squid.txt.html
 * http://www.ciscopress.com/articles/article.asp?p=1192686&seqNum=2
 */

#include "wccpv2.h"
#include "list.h"

/**
 * Do be honest I am not sure what ones of these I am actually going to need
 * but I will define everything here as described in the RFC draft.
 */

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
	__32  service_flags;
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




