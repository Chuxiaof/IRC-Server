#include "context.h"

#include "user.h"

struct worker_args {

    context_handle ctx;

    user_handle user_info;

};

typedef struct worker_args worker_args;

void *service_single_client(void *args);


