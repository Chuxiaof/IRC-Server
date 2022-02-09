#include "connection.h"

#include "log.h"

connection_handle create_connection(int socket_num)
{
    connection_handle res = calloc(1, sizeof(connection_t));
    if(res==NULL) {
        chilog(ERROR, "create connection: fail to allocate memory");
        exit(1);
    }
    res->socket_num = socket_num;
    res->state = UNKNOWN_CONNECTION;
    return res;
}

void destroy_connection(connection_handle connection)
{
    free(connection);
}