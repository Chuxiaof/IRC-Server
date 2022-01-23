#ifndef CONTEXT_H
#define CONTEXT_H

#include "user.h"

struct context{
    char *server_host;
    user_handle user_hash_table;
};

typedef struct context context;

typedef context * context_handle;

#endif