#ifndef CHANNEL_H
#define CHANNEL_H

#include <uthash.h>
#include <pthread.h>
#include <sds.h>
#include <stdbool.h>
#include <user.h>
#include "membership.h"

struct channel_t {
    char *name;

    // users on this channel
    membership_handle member_table;

    pthread_mutex_t mutex_member_table;

    // makes this structure hashable
    UT_hash_handle hh; 
};

typedef struct channel_t channel_t;

typedef channel_t * channel_handle;


channel_handle create_channel(char *name);

void destroy_channel(channel_handle channel);


bool already_on_channel(channel_handle channel, char *nick);

// 0: success
// -1: error
// 1: already on channel
int join_channel(channel_handle channel, char *nick, bool is_creator);

// 0: success
// -1: error
// 1: not on channel
// 2: empty channel
int leave_channel(channel_handle channel, char *nick);


int channel_member_count(channel_handle channel);

// caller need to free the return value
char **member_nicks_arr(channel_handle channel, int *count);

// caller need to free the return value
sds member_nicks_str(channel_handle channel);

// call already_on_channel before this function 
bool is_channel_operator(channel_handle channel, char *nick);

// -1: error
// 0 : success
// 1 : not on channel
// 2 : unsupported mode
int update_member_mode(channel_handle channel, char *nick, char *mode);

//0: success
//-1: error
//send message(reply) to all the members in the channel except the sender itself
// if there's no need to exclude the sender, set sender_nick argument as NULL
int send_to_channel_members(user_handle *hash_table, channel_handle channel, char *reply, char * sender_nick);


#endif