#ifndef CHANNEL_H
#define CHANNEL_H

#include <uthash.h>
#include <pthread.h>
#include <sds.h>
#include <stdbool.h>
#include <user.h>
#include "membership.h"

struct context_t;
typedef struct context_t context_t;
typedef context_t * context_handle;

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

/*
 * create a new channel:
 * allocate memory, initialize variables
 * 
 * name: name of the new channel
 * 
 * return: channel_handle
 */
channel_handle create_channel(char *name);

/*
 * free the memory
 */
void destroy_channel(channel_handle channel);

/*
 * check whether user is on current channel
 * 
 * channel: 
 * nick: nick of the user
 * 
 * return: true if on this channel
 *         false otherwise
 */
bool already_on_channel(channel_handle channel, char *nick);


/*
 * add a user to a channel
 * 
 * channel: 
 * nick: 
 * is_creator: if it's true, we will give the user operator 
 *              mode when adding
 * 
 * return:
 * 0: success
 * -1: error
 * 1: user already on this channel
 */
int join_channel(channel_handle channel, char *nick, bool is_creator);

/*
 * remove a user from the channel
 *
 * channel:
 * nick:
 * 
 * return:
 * 0: success
 * -1: error
 * 1: not on channel
 * 2: empty channel, this is used to tell the caller to delete this channel
 */
int leave_channel(channel_handle channel, char *nick);

/*
 * change the nick of the user on this channel
 * this is called when user updates his nick
 * 
 * channel: 
 * old_nick:
 * new_nick:
 * 
 * return:
 * 0: success
 * -1: error
 * 1: user not on this channel
 */
int update_member_nick(channel_handle channel, char *old_nick, char *new_nick);

/*
 * get the number of users on this channel
 * 
 * channel:
 * 
 * return: int
 */
int channel_member_count(channel_handle channel);

/*
 * get nicks of all users on this channel
 * 
 * channel:
 * count: this is used to store the number of users
 * 
 * return:
 * an array of string
 * (caller need to free the return value)
 */
char **member_nicks_arr(channel_handle channel, int *count);

/*
 * get nicks of all users on this channel
 * similar to last one
 * 
 * channel:
 * 
 * return:
 * sds, a string of all nicks, joined by ', '
 * (caller need to free the return value)
 */
sds member_nicks_str(channel_handle channel);

/* 
 * to check whether a user is the operator of current channel 
 * 
 * channel:
 * nick:
 * 
 * return: bool
 */
bool is_channel_operator(channel_handle channel, char *nick);

// -1: error
// 0 : success
// 1 : not on channel
// 2 : unsupported mode
/**
 * @brief update mode of a user on this channel
 * 
 * @param channel 
 * @param nick: user nick
 * @param mode: new mode
 * 
 * @return int:
 * -1: error
 * 0 : success
 * 1 : not on channel
 * 2 : unsupported mode
 */
int update_member_mode(channel_handle channel, char *nick, char *mode);

#endif