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
    char *client_host;
    
    // makes this structure hashable
    UT_hash_handle hh; 
};

typedef struct user user;

typedef user * user_handle;

user_handle create_user();

void delete_user(user_handle user);

void update_nick(context_handle ctx, user_handle user, char *nick);

void update_username(context_handle ctx, user_handle user, char *username, char *fullname);

// void send_welcome(user_handle user);


#endif