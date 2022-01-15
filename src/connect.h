#ifndef CONNECT_H
#define CONNECT_H

#include <stdbool.h>

struct connect_info
{
    char * nick;
    char * user;
    int server_fd;
    int client_fd;
    char * host_server;
    char * host_client;
    bool registered;
};

typedef struct connect_info connect_info;

typedef connect_info * connect_info_handle;

void send_welcome(connect_info_handle cinfo);

#endif