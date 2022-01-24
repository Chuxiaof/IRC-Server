#ifndef CONTEXT_H
#define CONTEXT_H

#include "user.h"

struct context_t {
    char *server_host;
    user_handle user_hash_table;
};

typedef struct context_t context_t;

typedef context_t * context_handle;

#endif