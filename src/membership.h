#ifndef MEMBERSHIP_H
#define MEMBERSHIP_H

#include <uthash.h>

struct membership_t {
    char * nick;

    bool is_channel_operator;
    
    UT_hash_handle hh; 
};

typedef struct membership_t membership_t;

typedef membership_t * membership_handle;

#endif