#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>

#include <sys/socket.h>

#include "clicommands.h"
#include "climanager.h"
#include "logger.h"

static struct command_head globalmode;
static struct command_head testmode;
static const char delimiters[] = " ";

void init_cli_global_mode(){
	sprintf(globalmode.prompt, "opennopd# ");
}

int cli_help(int client_fd, struct command_head *currentnode) {
    char msg[MAX_BUFFER_SIZE] = { 0 };
    struct command *currentcommand = NULL;
    int count = 1;

    currentcommand = currentnode->next;
    //sprintf(msg, "\n Showing help.\n");
    //cli_send_feedback(client_fd, msg);

    while (currentcommand) {

        if(currentcommand->hidden == false) {
            sprintf(msg, "[%d]: [%s] \n", count, currentcommand->command);
            cli_send_feedback(client_fd, msg);
            ++count;
        }
        currentcommand = currentcommand->next;
    }

    return 0;
}

struct commandresult cli_mode_test(int client_fd, char **parameters, int numparameters, void *data) {
    struct commandresult result = { 0 };

    result.finished = 0;
    result.mode = &testmode;
    result.data = NULL;

    return result;
}

struct commandresult cli_mode_test_exit(int client_fd, char **parameters, int numparameters, void *data) {
	struct commandresult result = { 0 };
    /*
     * Received a quit command so return 1 to shutdown this cli session.
     */
    result.finished = 0;
    result.mode = NULL;
    result.data = NULL;

    return result;
}

/*
 * Testing params
 */
struct commandresult cli_show_params(int client_fd, char **parameters, int numparameters, void *data) {
    int i = 0;
    struct commandresult result = { 0 };
    char msg[MAX_BUFFER_SIZE] = { 0 };

    for (i=0;i<numparameters;i++) {
        sprintf(msg, "[%d] %s\n", i, parameters[i]);
        cli_send_feedback(client_fd, msg);
    }

    result.finished = 0;
    result.mode = &testmode;
    result.data = NULL;

    return result;
}

void initializetestmode(){
    register_command(NULL, "test", cli_mode_test, false, true);
    register_command(&testmode, "exit", cli_mode_test_exit, false, false);
    register_command(&testmode, "show parameters", cli_show_params, true, false);
}

struct command* allocate_command() {
    struct command *newcommand = (struct command *) malloc (sizeof (struct command));
    if(newcommand == NULL) {
        fprintf(stdout, "Could not allocate memory... \n");
        exit(1);
    }
    newcommand->next = NULL;
    newcommand->prev = NULL;
    newcommand->child.next = NULL;
    newcommand->child.prev = NULL;
    pthread_mutex_init(&newcommand->child.lock,NULL);
    newcommand->command_handler = NULL;
    newcommand->hasparams = false;
    return newcommand;
}

/*
 */
struct command* find_command(struct command_head *currentnode, char *command_name) {
    struct command *currentcommand;
    currentcommand = currentnode->next;

    while(currentcommand != NULL ) {

        if (strcmp(currentcommand->command, command_name) == 0) {
            return currentcommand;
        }
        currentcommand = currentcommand->next;
    }

    return NULL;
}

/*
 * This replaces cli_process_message().
 * It can execute multiple commands that share the same name.
 */
struct commandresult execute_commands(struct command_head *mode, void *data, int client_fd, const char *command_name, int d_len) {
    char *token, *cp, *saved_token;
    int parametercount = 0;
    char **parameters = NULL; //dynamic array of pointers to tokens that are parameters
    char **tempparameters = NULL;
    char *parameter = NULL;
    struct command_head *currentnode = NULL;
    struct command *currentcommand = NULL;
    struct command *executedcommand = NULL;
    struct commandresult result = {0};
    char message[LOGSZ];

    sprintf(message, "CLI: Begin processing a command.\n");
    logger(LOG_INFO, message);

    if(mode == NULL) {
        currentnode = &globalmode;
    } else {
        currentnode = mode;
        result.mode = mode;
    }

    //if((head == NULL) || (head->next == NULL)) {
    if(currentnode->next == NULL) {
        /*
         * Cannot execute any commands if there are none.
         */
        sprintf(message, "CLI: No known commands.\n");
        logger(LOG_INFO, message);

        result.finished = 0;
        result.data = NULL;

        return result;
    }

    /*
     * Make writable copy of *command_name for separating into GNU string TOKENS.
     * The TOKENS are separated by spaces.
     */
    cp = strdup(command_name);

    /*
     * Initialize TOKEN.
     * The delimiters are globally defined for consistency.
     */
    token = strtok_r(cp, delimiters, &saved_token);

    while((token != NULL) && (executedcommand == NULL)) {

        if((strcmp(token,"help") == 0) || (strcmp(token,"?") == 0)) {
            cli_help(client_fd, currentnode);
            break;
        }
        currentcommand = find_command(currentnode,token);

        if(currentcommand == NULL) {
            /*
             * We didn't find any commands.
             * Show help for the current node then break.
             * Need the help function to accept the currentnode as a parameter.
             */
            sprintf(message, "CLI: Cound'nt find the command.\n");
            logger(LOG_INFO, message);
            cli_help(client_fd, currentnode);
            break;
        } else {

            if(currentcommand->child.next != NULL) {
                sprintf(message, "CLI: Command [%s] has children.\n", token);
                logger(LOG_INFO, message);
                currentnode = &currentcommand->child;
                currentcommand = NULL;
            } else {
                /*
                 * We found a command in this node.
                 * There are no other child nodes so its the last one.
                 */
                sprintf(message, "CLI: Locating [%s] command.\n", token);
                logger(LOG_INFO, message);

                while((executedcommand == NULL) && (currentcommand != NULL)) {
                    sprintf(message, "CLI: Current command [%s].\n", currentcommand->command );
                    logger(LOG_INFO, message);

                    if(currentcommand->command_handler == NULL) {
                        sprintf(message, "CLI: Command has no handler.\n");
                        logger(LOG_INFO, message);
                    }

                    if((!strcmp(currentcommand->command,token)) && (currentcommand->command_handler != NULL)) {

                        if(currentcommand->hasparams == true) {
                            sprintf(message, "CLI: Command has parameters.\n");
                            logger(LOG_INFO, message);
                            /*
                             * NOTE: I found that you cannot pass *cp because its modified by strtok_r()
                             * The modification removed the " " delimiter and must replace it with a \0
                             * to terminate that TOKEN.  This prevents *cp from being re-parsed as a parameter.
                             * As a solution we can pass the original command_name and try to process it more then.
                             * Another solution might to be make another copy of command_name then remove the
                             * original command somehow to pass only the remaining arguments/parameters.
                             * This will require saving the whole original command in the command structure
                             * so it can be referenced again.
                             *
                             * UPDATE: I posted a question at LinuxQuestions about this.
                             * One very good idea is to create a dynamic array of the TOKENs and
                             * pass that to the function as the parameter.
                             * http://www.linuxquestions.org/questions/programming-9/parse-string-tokens-and-pass-remaining-as-parameter-4175468498/#post4984764
                             */

                            token = strtok_r(NULL, delimiters, &saved_token); //Fetch the next TOKEN of the command.

                            if(token != NULL) {
                                /*
                                 * Grab the next token if any and initialize the parameter array.
                                 */
                                parameters = (char *)malloc(sizeof (parameter)); // Don't use *parameters it breaks.
                                parameters[0]= token;
                                parametercount = 1;
                            }

                            while(token != NULL) {
                                token = strtok_r(NULL, delimiters, &saved_token); //Fetch the next TOKEN of the command.

                                if(token != NULL) {
                                    tempparameters = (char *)realloc(parameters,(parametercount+1) * sizeof (parameter)); // Don't use *tempparameters it breaks.
                                    parameters = tempparameters;
                                    parameters[parametercount]=token;
                                    parametercount++ ;
                                }
                            }

                            result = (currentcommand->command_handler)(client_fd, parameters, parametercount, data);
                        } else {
                            /*
                             * We might want to verify no other TOKENs are left.
                             * If the command has no params none should have been given.
                             */
                            sprintf(message, "CLI: Command has no parameters.\n");
                            logger(LOG_INFO, message);
                            result = (currentcommand->command_handler)(client_fd, NULL, 0, data);
                        }
                        executedcommand = currentcommand;
                    } else {
                        sprintf(message, "CLI: Command did not match.\n");
                        logger(LOG_INFO, message);
                    }
                    currentcommand = currentcommand->next;
                }
                /*
                 * Finished executing all commands in the node.
                 */
                if (parameters != NULL) {
                    free(parameters);
                }
            }
        }

        token = strtok_r(NULL, delimiters, &saved_token); //Fetch the next TOKEN of the command.
    }

    if(result.finished != 1) { // Don't show the last prompt if we are done.
        //cli_prompt(client_fd);
    	cli_mode_prompt(client_fd, result.mode);
    }

    return result;
}

/*
 * This needs to be re-written to store the commands in a tree.
 * NOTE: Calls to strtok() do not seem to be thread safe.
 * So calls to register_command() are globally protected
 * with a mutex lock.  This will be the same lock used to
 * parse parameters for command.  Unless someone helps me make
 * it thread safe.  Maybe strtok_r()?
 * UPDATE: I am trying to use strtok_r().  It seems to be working.
 */

/** @brief Register a new command to the CLI.
 *
 * This function will register a new CLI command with the CLI module.
 *
 * @param mode [in] This command will enter a sub-mode with its own set of commands.  Should be NULL in most cases.
 * @param command_name [in] The string that will represent the command.  Should be unique "do something|show something".
 * @param handler_function [in] Function that will execute when the command is entered.
 * @param hasparams [in] If the parameter has parameters.  These must be checked by the handler_function.
 * @param hidden [in] Sets if the command shows in the default help context.
 * @return int
 */
int register_command(struct command_head *mode, const char *command_name, t_commandfunction handler_function, bool hasparams, bool hidden) {
    char *token, *cp, *saved_token;
    struct command_head *currentnode = NULL;
    struct command *currentcommand = NULL;
    char message[LOGSZ];

    //pthread_mutex_lock(&lock);
    sprintf(message, "CLI: Begin registering [%s] command.\n", command_name);
    logger(LOG_INFO, message);

    if(mode == NULL) {
        currentnode = &globalmode;
    } else {
        currentnode = mode;
    }

    /*
     * Make writable copy of *command_name for separating into GNU string TOKENS.
     * The TOKENS are separated by spaces.
     */
    cp = strdup(command_name);

    /*
     * Initialize TOKEN.
     * The delimiters are globally defined for consistency.
     */
    token = strtok_r(cp, delimiters, &saved_token);




    while(token != NULL) {
        pthread_mutex_lock(&currentnode->lock); //Prevent race condition when saving command tree.
        sprintf(message, "CLI: Register [%s] token.\n",token);
        logger(LOG_INFO, message);
        /*
         * Search the current node for a
         * command matching the current TOKEN.
         */
        currentcommand = find_command(currentnode,token);

        if(currentcommand != NULL) {
            /*!
             *Found the command for the current TOKEN.
             *Set it as the current node and search it for the next TOKEN.
             */
            sprintf(message, "CLI: Found an existing token.\n");
            logger(LOG_INFO, message);
        } else {
            /*
             * Did not find the command for the current TOKEN
             * We have to create it.
             */
            sprintf(message, "CLI: Did not find an existing token.\n");
            logger(LOG_INFO, message);
            currentcommand = allocate_command();
            currentcommand->command = token;

            if(currentnode->next == NULL) {
                sprintf(message, "CLI: Creating first command in node.\n");
                logger(LOG_INFO, message);
                currentnode->next = currentcommand;
                currentnode->prev = currentcommand;
            } else {
                sprintf(message, "CLI: Creating new command in node.\n");
                logger(LOG_INFO, message);
                currentnode->prev->next = currentcommand;
                currentcommand->prev = currentnode->prev;
                currentnode->prev = currentcommand;
            }
        }
        pthread_mutex_unlock(&currentnode->lock);
        currentnode = &currentcommand->child;
        token = strtok_r(NULL, delimiters, &saved_token); //Fetch the next TOKEN of the command.
    }

    //This was the last TOKEN of the command so assign the function here.
    if(currentcommand != NULL) {
        currentcommand->command_handler = handler_function;
        currentcommand->hasparams = hasparams;
        currentcommand->hidden = hidden;
    }

    //pthread_mutex_unlock(&lock);

    return 0;
}

/*
 * Show the opennopd# prompt.
 */
int cli_prompt(int client_fd) {
    char msg[MAX_BUFFER_SIZE] = { 0 };
    sprintf(msg, "opennopd# ");
    cli_send_feedback(client_fd, msg);
    return 0;
}

/*
 * Show the opennop->mode# prompt.
 */
int cli_mode_prompt(int client_fd, struct command_head *mode) {
    char msg[MAX_BUFFER_SIZE] = { 0 };
    char message[LOGSZ];

    sprintf(message, "Entering cli_mode_prompt().\n");
    logger(LOG_INFO, message);

    if(mode != NULL){
    	sprintf(msg, mode->prompt);
    	cli_send_feedback(client_fd, msg);
    }else{
    	cli_prompt(client_fd);
    }

    return 0;
}

void bytestostringbps(char *output, __u32 count) {
    int I = 0;
    int D = 0;
    int bits = 0;
    bits = count * 8; // convert bytes to bps.

    if (bits < 1024) { // output as bits.
        sprintf(output, "%i bps", bits);

        return;
    }

    if (((bits / 1024) / 1024) >= 1024) { // output as Gbps.
        I = ((bits / 1024) / 1024) / 1024;
        D = (((bits / 1024) / 1024) % 1024) / 10;
        sprintf(output, "%i.%i Gbps", I, D);

        return;
    }

    if ((bits / 1024) >= 1024) { // output as Mbps.
        I = (bits / 1024) / 1024;
        D = ((bits / 1024) % 1024) / 10;
        sprintf(output, "%i.%i Mbps", I, D);

        return;
    }

    if (bits >= 1024) { // output as Kbps.
        I = bits / 1024;
        D = (bits % 1024) / 10;
        sprintf(output, "%i.%i Kbps", I, D);

        return;
    }

    return;
}

int cli_send_feedback(int client_fd, char *msg) {
    if ((send(client_fd, msg, strlen(msg), 0)) <= 0) {
        perror("[cli_manager]: send");
        return 1;
    }
    return 0;
}
