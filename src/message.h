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

int message_from_string(message_handle msg, char *s);

int message_to_string(message_handle msg, char *s);

int message_construct(message_handle msg, char *prefix, char *cmd);

int message_add_parameter(message_handle msg, char * param, bool longlast);

int message_destroy(message_handle msg);

#endif