#ifndef CONTEXT_H
#define CONTEXT_H

#include <pthread.h>

#include "user.h"
#include "connection.h"
#include "channel.h"

#define SUCCESS 0
#define FAILURE -1

#define NICK_IN_USE 1

struct context_t {
    char *server_host;

    char *password;

    int irc_op_num;
    pthread_mutex_t mutex_op_num;

    connection_handle connection_hash_table;
    pthread_mutex_t mutex_connection_table;

    user_handle user_hash_table;
    pthread_mutex_t mutex_user_table;

    channel_handle channel_hash_table;
    pthread_mutex_t mutex_channel_table;
};

typedef struct context_t context_t;

typedef context_t * context_handle;

// context

/**
 * @brief Create a context object
 * 
 * @param password 
 * @return context_handle 
 */
context_handle create_context(char *password);

/**
 * @brief free the memory of a context struct 
 * 
 * @param ctx 
 */
void destroy_context(context_handle ctx);

// op_num

/**
 * @brief increase the number of operators of current server(context) by 1
 * 
 * @param ctx 
 * @return int 
 */
int increase_op_num(context_handle ctx);

// connection
/**
 * @brief add a new connection to the connext
 * 
 * @param ctx 
 * @param connection 
 * @return int 
 */
int add_connection(context_handle ctx, connection_handle connection);

/**
 * @brief modify the state of a connection
 * 
 * @param ctx 
 * @param id 
 * @param state: unknown, user, registered
 * @return int 
 */
int modify_connection_state(context_handle ctx, int id, int state);

/**
 * @brief remove a connection from current context
 * 
 * @param ctx 
 * @param socket_num 
 * @return int 
 */
int delete_connection(context_handle ctx, int socket_num);

/**
 * @brief count the number of different kinds of connections
 * 
 * @param ctx 
 * @return int*: an int array of size 3
 * arr[0]: unknown connection
 * arr[1]: user
 * arr[2]: registered
 * 
 * need to free the return value
 */
int *count_connection_state(context_handle ctx);

// user
/**
 * @brief add nick to a user, this is used when the user hasn't registered
 * 
 * @param ctx 
 * @param nick 
 * @param user 
 * @return int: SUCCESS, FAILURE, NICK_IN_USE
 */
int add_user_nick(context_handle ctx, char *nick, user_handle user);

/**
 * @brief update nick of a registered user
 * 
 * @param ctx 
 * @param new_nick 
 * @param user_info 
 * @param arr: this param is used to store the array of channels affected,
 *             caller need to notify members of these channels of the update
 * @param count: to store the number of affected channels
 * @return int: SUCCESS, FAILURE
 */
int update_user_nick(context_handle ctx, char *new_nick, user_handle user_info, channel_handle **arr, int *count);

/**
 * @brief Get the user object by nick
 * 
 * @param ctx 
 * @param nick 
 * @return user_handle 
 */
user_handle get_user(context_handle ctx, char *nick);

/**
 * @brief delete a user from the context
 * 
 * @param ctx 
 * @param user 
 * @return int 
 */
int delete_user(context_handle ctx, user_handle user);

// channel
/**
 * @brief Get the number of channels
 * 
 * @param ctx 
 * @return int 
 */
int get_channel_count(context_handle ctx);

/**
 * @brief Get the channel object by channel name
 * 
 * @param ctx 
 * @param name 
 * @return channel_handle 
 */
channel_handle get_channel(context_handle ctx, char *name);

/**
 * @brief compared to last function, this one will create a new channel
 *        if there doesn't exist one; this is designed for JOIN command
 * 
 * @param ctx 
 * @param name 
 * @param is_creator: true if newly created, false otherwise
 * @return channel_handle 
 */
channel_handle try_get_channel(context_handle ctx, char *name, bool *is_creator);

/**
 * @brief Get the channels user on 
 * 
 * @param ctx 
 * @param nick 
 * @param count: to store the number of channels
 * @return channel_handle*: an array of channels
 */
channel_handle *get_channels_user_on(context_handle ctx, char *nick, int *count);

/**
 * @brief Get the names of all channels
 * 
 * @param ctx 
 * @param count: to store the number of channels
 * @return char**: an array of string
 * need to free the return value
 */
char **get_all_channel_names(context_handle ctx, int *count);

#endif