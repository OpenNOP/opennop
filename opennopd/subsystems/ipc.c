#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h> // for multi-threading
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>

#include <linux/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <uuid/uuid.h>

#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "ipc.h"
#include "clicommands.h"
#include "logger.h"
#include "sockets.h"

typedef int (*t_neighbor_command)(int, __u32, char *);

/*
 * I was using "head" and "tail" here but that seemed to conflict with another module.
 * Should the internal variable names be isolated between modules?
 * They don't appear to be clicommands.c also uses a variable "head".
 * Using "static" should fix this.
 */
static pthread_t t_ipc; // thread for cli.
static struct neighbor_head ipchead;
static char UUID[OPENNOP_IPC_UUID_LENGTH]; //Local UUID.
static char key[OPENNOP_IPC_KEY_LENGTH]; //Local key.


static int DEBUG_IPC = LOGGING_OFF;

int ipc_send_message(int socket, OPENNOP_IPC_MSG_TYPE messagetype);
int update_neighbor_timer(struct neighbor *thisneighbor);

/**
 * Searches the neighbors list for an IP.
 * Returns NULL if no match is found.
 */
struct neighbor *find_neighbor_by_addr(struct in_addr *addr) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    while (currentneighbor != NULL) {

        if(currentneighbor->NeighborIP == addr->s_addr) {
            return currentneighbor;
        }
        currentneighbor = currentneighbor->next;
    }
    return NULL;
}

struct neighbor *find_neighbor_by_u32(__u32 neighborIP) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            return currentneighbor;
        }

        currentneighbor = currentneighbor->next;
    }
    return NULL;
}

struct neighbor *find_neighbor_by_socket(int fd) {
    int error = 0;
    socklen_t len;
    struct sockaddr_storage address;
    struct sockaddr_in *t = NULL;
    char ipstr[INET_ADDRSTRLEN];
    char message[LOGSZ] = {0};

    len = sizeof(address);
    error = getpeername(fd, (struct sockaddr*)&address, &len);

    if ((address.ss_family == AF_INET) && (error == 0)) {
        t = (struct sockaddr_in *)&address;
        inet_ntop(AF_INET, &t->sin_addr, ipstr, sizeof(ipstr));
    }
    sprintf(message, "[IPC] Peer IP address: %s\n", ipstr);
    logger2(LOGGING_INFO,DEBUG_IPC,message);

    if(t != NULL) {
        return find_neighbor_by_addr(&t->sin_addr);
    } else {
        return NULL;
    }
}

/**
 * This will generate the securitydata field of the OpenNOP messages.
 * The messages should already be encrypted at this point.
 * The securitydata field will be 0's before this operation.
 * The next operation should be sending the message.
 * @see http://linux.die.net/man/3/evp_sha256
 * @see http://www.openssl.org/docs/crypto/hmac.html
 * @see http://stackoverflow.com/questions/242665/understanding-engine-initialization-in-openssl
 */
int calculate_hmac_sha256(struct opennop_ipc_header *data, char *key, char *result) {
    unsigned int result_len = 32;
    HMAC_CTX ctx;

    ENGINE_load_builtin_engines();
    ENGINE_register_all_complete();
    HMAC_CTX_init(&ctx);
    HMAC_Init_ex(&ctx, key, 64, EVP_sha256(), NULL);
    HMAC_Update(&ctx, (unsigned char*)data, data->length);
    HMAC_Final(&ctx, (unsigned char*)result, &result_len);
    HMAC_CTX_cleanup(&ctx);
    return 0;
}

void sha256_to_string() {}

int ipc_neighbor_up(int fd) {
    struct neighbor *thisneighbor = NULL;
    thisneighbor = find_neighbor_by_socket(fd);

    if(thisneighbor != NULL) {
        thisneighbor->state = UP;
        update_neighbor_timer(thisneighbor);
    }
    return 0;
}

int get_header_data(struct opennop_ipc_header *opennop_msg_header,  struct opennop_header_data *data) {
    /*
     * This just reserves space for the HMAC calculation.
     */
    if(opennop_msg_header->security == 1) {
        data->securitydata = (char *)opennop_msg_header + sizeof(struct opennop_ipc_header);
        data->messages = data->securitydata + 32;
        opennop_msg_header->length = opennop_msg_header->length + 32;
    } else {
        data->securitydata = NULL;
        data->messages = (char *)opennop_msg_header + sizeof(struct opennop_ipc_header);
    }
    return 0;
}

int ipc_tx_message(int socket, struct opennop_ipc_header *opennop_msg_header) {
    int error;
    char message[LOGSZ] = {0};

    logger2(LOGGING_WARN,DEBUG_IPC,"IPC: Sending a message\n");
    print_opennnop_header(opennop_msg_header);

    /**
     * @note Must ignore the signal typically caused when the remote stops responding by adding the MSG_NOSIGNAL flag.
     * @see http://stackoverflow.com/questions/1330397/tcp-send-does-not-return-cause-crashing-process
     */
    error = send(socket, opennop_msg_header, opennop_msg_header->length, MSG_NOSIGNAL);

    /**
     *@todo Verify data is sent.
     */
    if(error != opennop_msg_header->length) {
        sprintf(message, "[socket] Only send %u bytes!\n", (unsigned int)error);
        logger2(LOGGING_ERROR,DEBUG_IPC,message);
    }

    return error;
}

int process_message(int fd, struct opennop_ipc_header *opennop_msg_header) {
    struct opennop_header_data data;
    struct opennop_message_header *message_header;

    logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Processing message!\n");

    get_header_data(opennop_msg_header, & data);
    message_header = (struct opennop_message_header *)data.messages;
    switch(message_header->type) {
    case OPENNOP_IPC_HERE_I_AM:
        logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Message Type: OPENNOP_IPC_HERE_I_AM.\n");
        ipc_send_message(fd, OPENNOP_IPC_I_SEE_YOU);
        break;
    case OPENNOP_IPC_I_SEE_YOU:
        logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Message Type: OPENNOP_IPC_I_SEE_YOU.\n");
        /**
         * @todo change the neighbor state to UP.
         */
        ipc_neighbor_up(fd);
        break;
    case OPENNOP_IPC_AUTH_ERR:
        logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Message Type: OPENNOP_IPC_AUTH_ERR.\n");
        break;
    case OPENNOP_IPC_BAD_UUID:
        logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Message Type: OPENNOP_IPC_BAD_UUID.\n");
        break;
    case OPENNOP_IPC_DEDUP_MAP:
        logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Message Type: OPENNOP_IPC_DEDUP_MAP.\n");
        break;
    default:
        logger2(LOGGING_WARN,DEBUG_IPC,"[IPC] Message Type: Unknown!\n");
    }

    return 0;
}

int check_hmac_sha256(int fd, struct opennop_ipc_header *opennop_msg_header) {
    struct neighbor *currentneighbor = NULL;
    char securitydata[32] = {0};
    struct opennop_header_data data;
    char message[LOGSZ] = {0};

    currentneighbor = find_neighbor_by_socket(fd);

    if(currentneighbor != NULL) {

        data.securitydata = (char *)opennop_msg_header + sizeof(struct opennop_ipc_header);
        memcpy(&securitydata, data.securitydata, 32);
        memset(data.securitydata, 0, 32);
        calculate_hmac_sha256(opennop_msg_header, (char *)&currentneighbor->key, data.securitydata);

        if(memcmp(securitydata,data.securitydata,32)==0) { // Compare the two HMAC values.
            /*
             * TODO:
             * This needs to execute process_message() next.
             */
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Security passed HMAC check!.\n");
            return process_message(fd, opennop_msg_header);
        } else { // Failed security check!
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Security failed HMAC check!.\n");
            return -1;
        }
    } else {
        return -1;
    }

}

int validate_security(int fd, struct opennop_ipc_header *opennop_msg_header, OPENNOP_MSG_SECURITY security) {
    char message[LOGSZ] = {0};

    if(opennop_msg_header->security == security) { // Security matched.  Next check.

        if(opennop_msg_header->security == OPENNOP_MSG_SECURITY_NO) { // No security required process message.
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Security passed MODE check!.\n");
            return process_message(fd, opennop_msg_header);
        } else {
            /*
             * This next function should test the HMAC data stored in the message header.
             */
            return check_hmac_sha256(fd, opennop_msg_header);
        }
    } else { // Security mismatch!
        logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Security failed MODE check!.\n");
        return -1;
    }
}

/**
 * Check if the message has a security component.
 * TODO: 1st Check if security is enabled.
 *       2nd Check message for security.
 *
 */
int check_security(int fd, struct opennop_ipc_header *opennop_msg_header) {
    struct neighbor *currentneighbor = NULL;
    currentneighbor = find_neighbor_by_socket(fd);

    if(currentneighbor != NULL) {

        if (strcmp(currentneighbor->key, "") == 0) { // No security required.
            return validate_security(fd, opennop_msg_header, OPENNOP_MSG_SECURITY_NO);
        } else { // Security required.
            return validate_security(fd, opennop_msg_header, OPENNOP_MSG_SECURITY_SHA);
        }
    } else {
        return -1;
    }
}

/**
 * @see http://linux.die.net/man/3/uuid_generate
 */
int ipc_add_i_see_you_message(struct opennop_ipc_header *opennop_msg_header) {
    struct ipc_message_i_see_you *message;

    message = (char*)opennop_msg_header + opennop_msg_header->length;
    message->header.type = OPENNOP_IPC_I_SEE_YOU;
    message->header.length = sizeof(struct ipc_message_i_see_you);
    /**
     * There is no additional data right now.
     */
    opennop_msg_header->length += message->header.length;

    return 0;
}

/**
 * @see http://linux.die.net/man/3/uuid_generate
 */
int add_hello_message(struct opennop_ipc_header *opennop_msg_header) {
    struct opennop_hello_message *message;

    message = (char*)opennop_msg_header + opennop_msg_header->length;
    message->header.type = OPENNOP_IPC_HERE_I_AM;
    message->header.length = sizeof(struct opennop_hello_message);
    /**
     *TODO: Generating the UUID should be done when the IPC module starts.
     *TODO: After its generated we should just copy it from the UUID variable.
     */
    uuid_generate_time((unsigned char*)message->uuid);
    opennop_msg_header->length += message->header.length;

    return 0;
}

int set_opennop_message_security(struct opennop_ipc_header *opennop_msg_header) {
    char message[LOGSZ] = {0};

    if (strcmp(key, "") == 0) {
        opennop_msg_header->security = OPENNOP_MSG_SECURITY_NO;
        logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] NO security.\n");
    } else {
        opennop_msg_header->security = OPENNOP_MSG_SECURITY_SHA;
        logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] SHA security.\n");
    }

    return 0;
}

int initialize_opennop_ipc_header(struct opennop_ipc_header *opennop_msg_header) {
    opennop_msg_header->type = OPENNOP_MSG_TYPE_IPC;
    opennop_msg_header->version = OPENNOP_MSG_VERSION;
    opennop_msg_header->length = OPENNOP_DEFAULT_HEADER_LENGTH; // Header is at least 8 bytes.
    set_opennop_message_security(opennop_msg_header);
    opennop_msg_header->antireplay = OPENNOP_MSG_ANTI_REPLAY_NO;
    return 0;
}

int print_opennnop_header(struct opennop_ipc_header *opennop_msg_header) {
    struct opennop_header_data data;
    char message[LOGSZ] = {0};
    char securitydata[33] = {0};

    sprintf(message, "Type: %u\n", opennop_msg_header->type);
    logger2(LOGGING_WARN,DEBUG_IPC,message);
    sprintf(message, "Version: %u\n", opennop_msg_header->version);
    logger2(LOGGING_WARN,DEBUG_IPC,message);
    sprintf(message, "Length: %u\n", opennop_msg_header->length);
    logger2(LOGGING_WARN,DEBUG_IPC,message);
    sprintf(message, "Security: %u\n", opennop_msg_header->security);
    logger2(LOGGING_WARN,DEBUG_IPC,message);
    sprintf(message, "Anti-Replay: %u\n", opennop_msg_header->antireplay);
    logger2(LOGGING_WARN,DEBUG_IPC,message);

    if(opennop_msg_header->security == 1) {
        data.securitydata = (char *)opennop_msg_header + sizeof(struct opennop_ipc_header);
        memset(&securitydata, 0, sizeof(securitydata));
        memcpy(&securitydata, data.securitydata, 32);
        sprintf(message, "Security Data: %s\n", securitydata);
        logger2(LOGGING_WARN,DEBUG_IPC,message);
    }

    return 0;
}

int update_neighbor_timer(struct neighbor *thisneighbor) {
    time_t currenttime;

    time(&currenttime);
    thisneighbor->timer = currenttime;
    return 0;
}

/*
 * This function is called from epoll_handler() in sockets.c
 * as defined in ipc_thread() in ipc.c.
 *
 * It functions as an ACL and ensures new connections originate from an IP
 * address of a neighbor in the neighbors list.
 *
 * Returns 0 if security check failed.
 * Returns 1 if security check passed.
 */
int ipc_check_neighbor(struct epoll_server *epoller, int fd, void *buf) {
    struct neighbor *thisneighbor = NULL;

    char message[LOGSZ] = {0};


    thisneighbor = find_neighbor_by_socket(fd);

    if(thisneighbor != NULL) {
        logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Found a neighbor!\n");
        thisneighbor->sock = fd;
        thisneighbor->state = ATTEMPT;
        update_neighbor_timer(thisneighbor);
        return 1;
    }

    /*
     * Failed to find a neighbor.
     */
    return 0;
}

/*
 * This function is called from epoll_handler() in sockets.c
 * as defined in ipc_thread() in ipc.c.
 *
 * It processes all messages received from neighbors.
 *
 * Returns 0 on successful completion.
 * Return -1 on failure. (should close socket)
 */
int ipc_handler(struct epoll_server *epoller, int fd, void *buf) {
    struct opennop_ipc_header *opennop_msg_header = NULL;
    char message[LOGSZ] = {0};
    /*
     *TODO: Here we need to check the message type and process it.
     *TODO: The epoll server should have a 2nd callback function that verifies the security of a connection.
     *TODO: So data passed from the epoll server to the handler *should* be considered secure.
     */
    logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Received a message\n");

    if(buf != NULL) {

        /*
         * Write the message structure to log for testing.
         */
        print_opennnop_header((struct opennop_ipc_header *)buf);

        opennop_msg_header = buf;

        /*
         * Check the type and process it with the correct function.
         * Currently there is only one type being handled.
         */
        switch(opennop_msg_header->type) {
        case OPENNOP_MSG_TYPE_IPC:
            /*
             * To check the security we will pass the socket this message came from.
             * Also epoller is the instance of epoll used to handle this message.
             */
            return check_security(fd, opennop_msg_header);
            break;
        default: //non-standard type.
            break;
        }
    }

    return 0;
}

int ipc_send_message(int socket, OPENNOP_IPC_MSG_TYPE messagetype) {
    struct opennop_header_data data;
    struct opennop_ipc_header *opennop_msg_header;
    char buf[IPC_MAX_MESSAGE_SIZE] = {0};
    int error;
    char message[LOGSZ] = {0};

    /*
     * Setting up the OpenNOP Message Header.
     */
    opennop_msg_header = (struct opennop_ipc_header *)&buf;
    initialize_opennop_ipc_header(opennop_msg_header);

    get_header_data(opennop_msg_header, &data);

    switch(messagetype) {
    case OPENNOP_IPC_HERE_I_AM:
        add_hello_message(opennop_msg_header);
        break;
    case OPENNOP_IPC_I_SEE_YOU:
        ipc_add_i_see_you_message(opennop_msg_header);
        break;
    default:
        logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Cannot send unknown type!\n");
    }

    /*
     * Now after all headers are added we calculate the HMAC.
     */
    if(opennop_msg_header->security == 1) {
        calculate_hmac_sha256(opennop_msg_header, (char *)&key, data.securitydata);
    }

    return ipc_tx_message(socket,opennop_msg_header);

}

int hello_neighbors(struct epoll_server *epoller) {
    struct neighbor *currentneighbor = NULL;
    time_t currenttime;
    int error = 0;
    int newsocket = 0;
    char message[LOGSZ] = {0};

    time(&currenttime);

    logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Starting hello_neighbors().\n");

    for(currentneighbor = ipchead.next; currentneighbor != NULL; currentneighbor = currentneighbor->next) {
        logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Found at least one neighbor.\n");

        switch (currentneighbor->state) {
        case DOWN:
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Neighbor is DOWN.\n");
            break;
        case ATTEMPT:
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Neighbor is ATTEMPT.\n");
            break;
        default:
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Neighbor is in an unknown state.\n");
            break;
        }
        if(currentneighbor->sock > 0) {
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Neighbor has a valid socket.\n");
        }

        if((currentneighbor->state == DOWN) && (difftime(currenttime, currentneighbor->timer) >= 30)) {
            currentneighbor->timer = currenttime;
            logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Neighbor state is down & timer > 30\n");

            /*
             * If neighbor socket = 0 open a new one.
             */
            if(currentneighbor->sock == 0) {
                newsocket = new_ip_client(currentneighbor->NeighborIP,OPENNOPD_IPC_PORT);

                if(newsocket > 0) {
                    currentneighbor->sock = newsocket;
                    currentneighbor->state = ATTEMPT;
                    /*
                     * This socket has to be registered with the epoll server.
                     */
                    register_socket(newsocket, epoller->epoll_fd, &epoller->event);

                }
            }

        } else if((currentneighbor->state >= ATTEMPT) && (difftime(currenttime, currentneighbor->timer) >= 10)) {

            if(currentneighbor->sock != 0) {
                currentneighbor->timer = currenttime;
                error = ipc_send_message(currentneighbor->sock,OPENNOP_IPC_HERE_I_AM);

                /**
                 * @todo Maybe this should be moved to ipc_send_message()?
                 */
                if (error < 0) {
                    logger2(LOGGING_DEBUG,DEBUG_IPC,"[IPC] Failed sending hello.\n");
                    currentneighbor->state = DOWN;
                    shutdown(currentneighbor->sock, SHUT_RDWR);
                    currentneighbor->sock = 0;
                }
            }
        }
    }
    return 0;
}

/*
 * This thread will monitor all IPC events.
 * It has to monitor in-bound connection for remote IPC
 * and also handle in-bound local IPC from the CLI.
 *
 * The CLI will use the local IPC to notify the IPC thread
 * that new fd's need to be setup and monitored.
 * For events like adding/removing neighbors from the IPC.
 *
 * Thanks to Mukund Sivaraman for the tutorial on epoll().
 * https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
 * http://man7.org/linux/man-pages/man7/epoll.7.html
 * http://www.cs.rutgers.edu/~pxk/rutgers/notes/sockets/
 * http://www.beej.us/guide/bgnet/output/html/multipage/fcntlman.html
 */
void *ipc_thread(void *dummyPtr) {
    int error = 0;
    struct epoll_server ipc_server = {
                                         0
                                     };
    char message[LOGSZ] = {0};

    logger2(LOGGING_INFO,DEBUG_IPC, "IPC: Is starting.\n");

    error = new_ip_epoll_server(&ipc_server, ipc_check_neighbor, ipc_handler, OPENNOPD_IPC_PORT, hello_neighbors, OPENNOP_IPC_HELLO);

    /*
     * This should not return until the epoll server is shutdown.
     */
    epoll_handler(&ipc_server);

    logger2(LOGGING_INFO,DEBUG_IPC, "IPC: Is exiting.\n");

    shutdown_epoll_server(&ipc_server);

    return EXIT_SUCCESS;
}

int cli_neighbor_help(int client_fd) {
    char msg[IPC_MAX_MESSAGE_SIZE] = { 0 };

    sprintf(msg,"Usage: [no] neighbor <ip address> [key]\n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

struct commandresult cli_show_neighbors(int client_fd, char **parameters, int numparameters, void *data) {
    struct neighbor *currentneighbor = NULL;
    struct commandresult result  = {
                                       0
                                   };
    char temp[20];
    char col1[17];
    char col2[33];
    char col3[66];
    char end[3];
    char msg[IPC_MAX_MESSAGE_SIZE] = { 0 };

    currentneighbor = ipchead.next;

    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);
    sprintf(
        msg,
        "|   Neighbor IP   |       GUID       |                               Key                                |\n");
    cli_send_feedback(client_fd, msg);
    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);

    while(currentneighbor != NULL) {
        strcpy(msg, "");
        inet_ntop(AF_INET, &currentneighbor->NeighborIP, temp,
                  INET_ADDRSTRLEN);
        sprintf(col1, "| %-16s", temp);
        strcat(msg, col1);
        sprintf(col2, "| %-17s", currentneighbor->UUID);
        strcat(msg, col2);
        sprintf(col3, "| %-65s", currentneighbor->key);
        strcat(msg, col3);
        sprintf(end, "|\n");
        strcat(msg, end);
        cli_send_feedback(client_fd, msg);

        currentneighbor = currentneighbor->next;
    }

    sprintf(
        msg,
        "---------------------------------------------------------------------------------------------------------\n");
    cli_send_feedback(client_fd, msg);

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

int cli_show_neighbor_help(int client_fd) {
    char msg[IPC_MAX_MESSAGE_SIZE] = { 0 };

    sprintf(msg,"Usage: show neighbor <ip address> \n");
    cli_send_feedback(client_fd, msg);

    return 0;
}

/** @brief Display a particular neighbors details.
 *
 * Print the details of a particular neighbor to a particular CLI session.
 *
 * @param client_fd [in] The CLI session that executed the command.
 * @param currentneighbor [in] The neighbor whos details to display.
 * @return int
 */
int cli_display_neighbor_details(int client_fd, struct neighbor *currentneighbor) {

    char strip[20];
    char msg[IPC_MAX_MESSAGE_SIZE] = { 0 };

    /**
     * @todo What data should be displayed from this neighbor?
     * STATE?
     * UUID?
     * KEY?
     */
    inet_ntop(AF_INET, &currentneighbor->NeighborIP, strip,INET_ADDRSTRLEN);
    sprintf(msg,"Neighbor: %s.\n",strip);
    cli_send_feedback(client_fd, msg);

    switch(currentneighbor->state) {
    case DOWN:
    	sprintf(msg,"state = down\n");
        break;
    case ATTEMPT:
    	sprintf(msg,"state = attempt\n");
        break;
    case ESTABLISHED:
    	sprintf(msg,"state = established\n");
        break;
    case UP:
    	sprintf(msg,"state = up\n");
        break;
    default:
    	sprintf(msg,"state = unknown?\n");
    }
    cli_send_feedback(client_fd, msg);

    return 0;
}

/** @brief CLI function to show a particular neighbor status.
 *
 * Finds a neighbor by IP and displays relevant info.
 *
 * @param client_fd [in] The CLI session that executed the command.
 * @param parameters[0] [in] The IP address in "0.0.0.0" format. (Verified by function)
 * @param numparameters [in] Should only be 1. (Verified by function)
 * @param data [in] Should be NULL.
 */
struct commandresult cli_show_neighbor(int client_fd, char **parameters, int numparameters, void *data) {
    int ERROR = 0;
    struct neighbor *currentneighbor = NULL;
    __u32 neighborIP = 0;
    struct commandresult result  = {
                                       0
                                   };

    /**
     * @note We always have to validate user input.
     * This command only accepts 1 parameter.
     * It should be an IP address and inet_pton should return 1 to validate it.
     */
    if(numparameters == 1) {
        ERROR = inet_pton(AF_INET, parameters[0], &neighborIP); // Should return 1.
    } else {
        cli_show_neighbor_help(client_fd);
    }

    if(ERROR != 1) {
        cli_show_neighbor_help(client_fd);
    } else {
        currentneighbor = find_neighbor_by_u32(neighborIP);
    }

    if(currentneighbor != NULL) {
        cli_display_neighbor_details(client_fd,currentneighbor);
    }

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;
    return result;
}

int set_neighbor_key(struct neighbor *currentneighbor, char *key) {
    memset(currentneighbor->key, 0, sizeof(currentneighbor->key));

    if(key != NULL) {
        strncpy(currentneighbor->key, key, strlen(key));
    }
    return 0;
}

struct neighbor* allocate_neighbor(__u32 neighborIP, char *key) {
    struct neighbor *newneighbor = (struct neighbor *) malloc (sizeof (struct neighbor));

    if(newneighbor == NULL) {
        fprintf(stdout, "Could not allocate memory... \n");
        exit(1);
    }
    newneighbor->next = NULL;
    newneighbor->prev = NULL;
    newneighbor->NeighborIP = 0;
    newneighbor->state = DOWN;
    newneighbor->UUID[0] = '\0';
    newneighbor->sock = 0;
    newneighbor->key[0] = '\0';
    time(&newneighbor->timer);

    if (neighborIP != 0) {
        newneighbor->NeighborIP = neighborIP;
    }

    if (key != NULL) {
        set_neighbor_key(newneighbor, key);
    }

    return newneighbor;
}



int add_update_neighbor(int client_fd, __u32 neighborIP, char *key) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    /*
     * Make sure the neighbor does not already exist.
     */
    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            set_neighbor_key(currentneighbor, key);

            return 0;
        }

        currentneighbor = currentneighbor->next;
    }

    /*
     * Did not find the neighbor so lets add it.
     */

    currentneighbor = allocate_neighbor(neighborIP, key);

    if (currentneighbor != NULL) {

        if (ipchead.next == NULL) {
            ipchead.next = currentneighbor;
            ipchead.prev = currentneighbor;

        } else {
            currentneighbor->prev = ipchead.prev;
            ipchead.prev->next = currentneighbor;
            ipchead.prev = currentneighbor;
        }

    }

    return 0;
}

int del_neighbor(int client_fd, __u32 neighborIP, char *key) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            if((currentneighbor->prev == NULL) && (currentneighbor->next == NULL)) {
                ipchead.next = NULL;
                ipchead.prev = NULL;

            } else if ((currentneighbor->prev == NULL) && (currentneighbor->next != NULL)) {
                ipchead.next = currentneighbor->next;
                currentneighbor->next->prev = NULL;

            } else if ((currentneighbor->prev != NULL) && (currentneighbor->next == NULL)) {
                ipchead.prev = currentneighbor->prev;
                currentneighbor->prev->next = NULL;

            } else if ((currentneighbor->next != NULL) && (currentneighbor->prev != NULL)) {
                currentneighbor->prev->next = currentneighbor->next;
                currentneighbor->next->prev = currentneighbor->prev;
            }

            free(currentneighbor);
            currentneighbor = NULL;

            return 0;
        }

        currentneighbor = currentneighbor->next;
    }

    return 0;
}

int validate_neighbor_input(int client_fd, char *stringip, char *key, t_neighbor_command neighbor_command) {
    int ERROR = 0;
    int keylength = 0;
    __u32 neighborIP = 0;
    char msg[IPC_MAX_MESSAGE_SIZE] = { 0 };

    /*
     * We must validate the user data here
     * before adding or updating anything.
     * 1. stringip should convert to an integer using inet_pton()
     * 2. key should be NULL or < 64 bytes
     */

    ERROR = inet_pton(AF_INET, stringip, &neighborIP); //Should return 1.

    if(ERROR != 1) {
        return cli_neighbor_help(client_fd);
    }

    if(key != NULL) {
        keylength = strlen(key);

        if(keylength > 64) {
            return cli_neighbor_help(client_fd);
        }
    }

    /*
     * The input is valid so we run the specified command
     * This will add, update or remove a neighbor.
     */
    neighbor_command(client_fd, neighborIP, key);

    /*
    sprintf(msg,"Neighbor string IP is [%s].\n", stringip);
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Neighbor integer IP is [%u].\n", ntohl(neighborIP));
    cli_send_feedback(client_fd, msg);
    sprintf(msg,"Neighbor key is [%s].\n", key);
    cli_send_feedback(client_fd, msg);

    if(key != NULL) {
        sprintf(msg,"Neighbor key length is [%u].\n", keylength);
        cli_send_feedback(client_fd, msg);
}
    */

    return 0;
}

/*
 * We only need to validate the number of parameters are correct here.
 * Lets let the more specific functions do the data validation from the input.
 * This should accept 1 parameter.
 * parameter[0] = IP in string format.
 */
struct commandresult cli_no_neighbor(int client_fd, char **parameters, int numparameters, void *data) {
    int ERROR = 0;
    struct commandresult result = {
                                      0
                                  };

    if (numparameters == 1) {
        ERROR = validate_neighbor_input(client_fd, parameters[0], NULL, &del_neighbor);

    } else {
        ERROR = cli_neighbor_help(client_fd);
    }

    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

/*
 * We only need to validate the number of parameters are correct here.
 * Lets let the more specific functions do the data validation from the input.
 * This should accept 1 or 2 parameters.
 * parameter[0] = IP in string format.
 * parameter[1] = Authentication key(optional).
 */
struct commandresult cli_neighbor(int client_fd, char **parameters, int numparameters, void *data) {
    int socket = 0;
    int ERROR = 0;
    struct commandresult result  = {
                                       0
                                   };
    char buf[IPC_MAX_MESSAGE_SIZE];
    char message[LOGSZ] = {0};

    result.mode = NULL;
    result.data = NULL;

    if ((numparameters < 1) || (numparameters > 2)) {
        ERROR = cli_neighbor_help(client_fd);

    } else if (numparameters == 1) {
        ERROR = validate_neighbor_input(client_fd, parameters[0], NULL, &add_update_neighbor);

    } else if (numparameters == 2) {
        ERROR = validate_neighbor_input(client_fd, parameters[0], parameters[1], &add_update_neighbor);
    }

    /**
     * The IPC does not respond to UNIX sockets anymore.
     */

    /*socket = new_unix_client(OPENNOPD_IPC_SOCK);

    if (socket < 0) {
        sprintf(message, "IPC: CLI failed to connect.\n");
        logger(LOG_INFO, message);
        result.finished = 1;
        close(socket);
        return result;
}

    sprintf(buf,"Hello!\n");
    ERROR = send(socket, buf, strlen(buf), 0);

    if (ERROR < 0) {
        sprintf(message, "IPC: CLI could not write to IPC socket.\n");
        logger(LOG_INFO, message);
        result.finished = 1;
        close(socket);
        return result;
}
    ERROR = shutdown(socket, SHUT_WR);
    */

    /*
     * TODO: Here we should read from the socket for x seconds.
     * If we receive the proper shutdown from the server we can close the socket.
     */

    result.finished = 0;

    return result;
}

struct commandresult cli_set_key(int client_fd, char **parameters, int numparameters, void *data) {
    int ERROR = 0;
    int keylength = 0;
    struct commandresult result  = {
                                       0
                                   };
    char buf[IPC_MAX_MESSAGE_SIZE];
    char message[LOGSZ] = {0};

    result.mode = NULL;
    result.data = NULL;

    if (numparameters == 1) {

        if(parameters[0] != NULL) {
            keylength = strlen(parameters[0]);

            if(keylength > 64) {
                result.finished = 0;
                return result;
            }
            memset(key, 0, sizeof(key));
            strncpy(key, parameters[0], strlen(parameters[0]));

        }
    }

    if(numparameters == 0) {
        memset(key, 0, sizeof(key));
    }

    result.finished = 0;

    return result;
}

struct commandresult cli_show_key(int client_fd, char **parameters, int numparameters, void *data) {
    char msg[IPC_MAX_MESSAGE_SIZE] = { 0 };
    struct commandresult result  = {
                                       0
                                   };

    result.mode = NULL;
    result.data = NULL;
    sprintf(msg,"Key: [%s]\n",key);
    cli_send_feedback(client_fd, msg);
    result.finished = 0;
    return result;
}

/*
 * We need to verify that the remote accelerator is a member of this domain.
 * TODO: Later the UUID will need to be used instead of the IP address.
 */
int verify_neighbor_in_domain(__u32 neighborIP) {
    struct neighbor *currentneighbor = NULL;

    currentneighbor = ipchead.next;

    while (currentneighbor != NULL) {

        if (currentneighbor->NeighborIP == neighborIP) {

            return 1;
        }

        currentneighbor = currentneighbor->next;
    }

    return 0;
}

void start_ipc() {
    /*
     * This will setup and start the IPC threads.
     * We also register the IPC commands here.
     */
    register_command(NULL, "neighbor", cli_neighbor, true, false);
    register_command(NULL, "no neighbor", cli_no_neighbor, true, false);
    register_command(NULL, "show neighbors", cli_show_neighbors, false, false);
    register_command(NULL, "key", cli_set_key, true, true);
    register_command(NULL, "show key", cli_show_key, false, true);
    register_command(NULL, "show neighbor", cli_show_neighbor, true, true);

    ipchead.next = NULL;
    ipchead.prev = NULL;

    pthread_create(&t_ipc, NULL, ipc_thread, (void *) NULL);

}

void rejoin_ipc() {
    pthread_join(t_ipc, NULL);
}
