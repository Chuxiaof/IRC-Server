#include "context.h"
#include "user.h"


struct worker_args {

    context_handle ctx;

    user_handle user_info;

};

typedef struct worker_args worker_args;

/**
 * @brief This is the function that is run by the "worker thread".
   It is in charge of "handling" an individual connection, also parsing the message received 
   from the client side
 * 
 * @param args arguments passed from the process(start_server() function) to this particular thread
 * @return void* 
 */
void * service_single_client(void *args);


