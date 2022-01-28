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
context_handle create_context(char *password);

void destroy_context(context_handle ctx);

// op_num
int increase_op_num(context_handle ctx);

// connection
int add_connection(context_handle ctx, connection_handle connection);

int modify_connection_state(context_handle ctx, int id, int state);

int delete_connection(context_handle ctx, int socket_num);

// need to free the return value
int *count_connection_state(context_handle ctx);

// user
int add_user_nick(context_handle ctx, char *nick, user_handle user);

int update_user_nick(context_handle ctx, char *nick, user_handle user, channel_handle **arr, int *count);

user_handle get_user(context_handle ctx, char *nick);

int delete_user(context_handle ctx, user_handle user);

// channel
int get_channel_count(context_handle ctx);

channel_handle get_channel(context_handle ctx, char *name);

channel_handle try_get_channel(context_handle ctx, char *name, bool *is_creator);

channel_handle *get_channels_user_on(context_handle ctx, char *nick, int *count);

// need to free the return value
char **get_all_channel_names(context_handle ctx, int *count);

#endif