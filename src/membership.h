#ifndef MEMBERSHIP_H
#define MEMBERSHIP_H

#include <uthash.h>

/**
 * @brief this struct is used to represent the relationship
 * between channel and user, nick is the nick of the user, is_channel_operator denotes 
 * whether a user is an operator of current channel, a channel will maintain a hashmap
 * of membership_t to store all users on this channel
 * 
 */
struct membership_t {
    char * nick;    // key

    bool is_channel_operator;
    
    UT_hash_handle hh; 
};

typedef struct membership_t membership_t;

typedef membership_t * membership_handle;

#endif