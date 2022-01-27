#ifndef CONTEXT_H
#define CONTEXT_H

#include <pthread.h>

#include "user.h"
#include "connection.h"
#include "channel.h"

struct context_t {
    char *server_host;

    char * password;

    int irc_op_num;

    user_handle user_hash_table;
    pthread_mutex_t lock_user_table;

    connection_handle connection_hash_table;
    pthread_mutex_t lock_connection_table;

    channel_handle channel_hash_table;
    pthread_mutex_t lock_channel_table;
};

typedef struct context_t context_t;

typedef context_t * context_handle;

#endif