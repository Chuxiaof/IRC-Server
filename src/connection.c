#include "connection.h"

#include "log.h"

connection_handle create_connection(int socket_num)
{
    connection_handle res = malloc(sizeof(connection_t));
    if(res==NULL) {
        chilog(ERROR, "fail to create user: no enough memory");
        exit(1);
    }
    res->socket_num = socket_num;
    res->state = UNKNOWN_CONNECTION;
    return res;
}

void modify_connection(connection_handle * hash_table, int socket_num, int state)
{
    connection_handle connection;
    HASH_FIND_INT(*hash_table, &socket_num, connection);
    if((state!= REGISTERED_CONNECTION) && (state != USER_CONNECTION)) {
        chilog(ERROR, "false state info for connection");
        exit(1);
    }
    if(connection->state!=REGISTERED_CONNECTION) {
        connection->state = state;
    }
    return;
}

void delete_connection(connection_handle * hash_table, int socket_num)
{
    connection_handle connection;
    HASH_FIND_INT(*hash_table, &socket_num, connection);
    HASH_DEL(*hash_table, connection);
    free(connection);
}
