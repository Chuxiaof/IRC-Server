#ifndef CONTEXT_H
#define CONTEXT_H

#include "user.h"
#include "connection.h"
#include "channel.h"

struct context_t {
    char *server_host;
    user_handle user_hash_table;
    connection_handle connection_hash_table;
    channel_handle channel_hash_table;
};

typedef struct context_t context_t;

typedef context_t * context_handle;

#endif