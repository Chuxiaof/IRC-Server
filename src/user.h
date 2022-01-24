#ifndef USER_H
#define USER_H

#include <stdbool.h>
#include <uthash.h>

struct user
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

typedef struct user user;

typedef user * user_handle;

user_handle create_user();

void destroy_user(user_handle user);




#endif