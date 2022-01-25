#ifndef CHANNEL_H
#define CHANNEL_H

#include <uthash.h>

#include "user.h"

struct channel_t {
    char *name;

    // users on this channel
    user_handle user_table;

    // makes this structure hashable
    UT_hash_handle hh; 
};

typedef struct channel_t channel_t;

typedef channel_t * channel_handle;

channel_handle create_channel(char *name);

void join_channel(channel_handle channel, user_handle user);



#endif