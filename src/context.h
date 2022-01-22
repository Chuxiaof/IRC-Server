#include "user.h"

struct context{
    char *server_host;

    user_handle users_hash_table;
};

typedef struct context context;

typedef context * context_handle;

