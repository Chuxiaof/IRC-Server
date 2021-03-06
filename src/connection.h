#ifndef CONNECTION_H
#define CONNECTION_H

#include <uthash.h>

#define UNKNOWN_CONNECTION 0
#define USER_CONNECTION 1
#define REGISTERED_CONNECTION 2

struct connection_t {
    int socket_num; //key
    int state;  
    UT_hash_handle hh;
};

typedef struct connection_t connection_t;

typedef connection_t * connection_handle;

/**
 * @brief Create a connection object
 * 
 * @param socket_num 
 * @return connection_handle 
 */
connection_handle create_connection(int socket_num);

/**
 * @brief free the memory of a connection object
 * 
 * @param connection 
 */
void destroy_connection(connection_handle connection);

#endif
