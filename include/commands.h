#ifndef COMMANDS_H_
#define COMMANDS_H_

/**
 * For now the commands functions accept a message and must parse it.
 * I want to parse the message to and array of arguments in the messages module
 * Then pass that array through all the command functions.
 */
#include "messages.h"

/*
 * This is just a prototype of the function to be implemented in other modules.
 * Other modules should implement this function then register it with this module.
 * Sorry if my terms are incorrect!
 */
int commandfunction(struct msgbuf message);

/*
 * This function is called to register the command function from other modules.
 * The parameter should be a pointer to a commandfucntion.
 */
int registercommandfunction(int (*commandfunction)(struct msgbuf message));


#endif /*COMMANDS_H_*/
