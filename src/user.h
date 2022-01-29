#ifndef USER_H
#define USER_H

#include <stdbool.h>
#include <uthash.h>

/**
 * @brief Store the relevant information about user, 
   also contains the socket file descriptor of the connection
 * 
 */
struct user_t
{
    // the socket of connection
    int client_fd;
    char *client_host_name;
    
    char *nick;
    char *username;
    char *fullname;
    bool registered;
    bool is_irc_operator;
    
    // makes this structure hashable
    UT_hash_handle hh; 
};

typedef struct user_t user_t;

typedef user_t * user_handle;

/**
 * @brief Create a user object
 * 
 * @return user_handle 
 */
user_handle create_user();

/**
 * @brief free the memory
 * 
 * @param user 
 */
void destroy_user(user_handle user);

/**
 * @brief to check whether we can register current user
 * if both nick and username fields have value, the answer is true,
 * false otherwise
 * 
 * @param user 
 * @return true 
 * @return false 
 */
bool can_register(user_handle user);

#endif