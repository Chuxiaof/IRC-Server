#ifndef USER_H
#define USER_H

#include <stdbool.h>
#include <uthash.h>

struct user_t
{
    char *nick;
    char *username;
    char *fullname;
    bool registered;

    // the socket of connection
    int client_fd;
    char *client_host_name;
    
    // makes this structure hashable
    UT_hash_handle hh; 
};

typedef struct user_t user_t;

typedef user_t * user_handle;

user_handle create_user();

void destroy_user(user_handle user);

bool can_register(user_handle user);

// void register(user_handle user);

void send_welcome(user_handle user, char *server_host_name);


#endif