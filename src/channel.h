#ifndef CHANNEL_H
#define CHANNEL_H

#include <uthash.h>
#include <pthread.h>
#include <sds.h>

#include "user.h"

struct channel_t {
    char *name;

    // users on this channel
    user_handle user_table;
    pthread_mutex_t lock_channel_user;

    // makes this structure hashable
    UT_hash_handle hh; 
};

typedef struct channel_t channel_t;

typedef channel_t * channel_handle;

channel_handle create_channel(char *name);

bool already_on_channel(channel_handle channel, char *nick);

void join_channel(channel_handle channel, user_handle user);

// caller need to free the return value
sds all_user_nicks(channel_handle channel);

void leave_channel(channel_handle channel, user_handle user);

bool empty_channel(channel_handle channel);


#endif