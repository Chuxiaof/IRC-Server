#include "context.h"

#include "user.h"

struct worker_args {

    context_handle ctx;

    user_handle user_info;

};

void *service_single_client(void *args);


