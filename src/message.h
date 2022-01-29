#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdbool.h>

struct message_t {
    char *prefix;
    char *cmd;
    char *params[15];
    unsigned int nparams;
    bool longlast;
};

typedef struct message_t message_t;

typedef message_t * message_handle;

/**
 * @brief tokenize a string to a message object
 * 
 * @param msg 
 * @param s 
 * @return int 
 */
int message_from_string(message_handle msg, char *s);

/**
 * @brief convert a message object to a string
 * 
 * @param msg 
 * @param s 
 * @return int 
 */
int message_to_string(message_handle msg, char *s);

/**
 * @brief add values to a new message object
 * 
 * @param msg 
 * @param prefix 
 * @param cmd 
 * @param nick 
 * @return int 
 */
int message_construct(message_handle msg, char *prefix, char *cmd, char *nick);

/**
 * @brief add params to a message object
 * 
 * @param msg 
 * @param param 
 * @param longlast 
 * @return int 
 */
int message_add_parameter(message_handle msg, char * param, bool longlast);

/**
 * @brief free the memory of a message object
 * 
 * @param msg 
 * @return int 
 */
int message_destroy(message_handle msg);

#endif