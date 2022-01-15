#ifndef CONNECT_H
#define CONNECT_H

#include <stdbool.h>
#include <uthash.h>
#include <sds.h>

struct connect_info
{
    char * nick; 
    char * user; //username 
    int server_fd; //unnecessary?
    int client_fd; //the socket of connection
    char * host_server; 
    char * host_client;
    bool registered;
    UT_hash_handle hh; //makes this structure hashable
};

typedef struct connect_info connect_info;

typedef connect_info * connect_info_handle;

void send_welcome(connect_info_handle cinfo, connect_info_handle connections, sds * tokens);

void destroy_cinfo(connect_info_handle cinfo);

#endif