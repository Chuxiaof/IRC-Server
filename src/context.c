#include "context.h"

#include "log.h"

channel_handle *update_nick_on_channel(context_handle ctx, char *nick, int *count);

context_handle create_context(char *password) {
    context_handle ctx = calloc(1, sizeof(context_t));
    if (ctx == NULL) {
        chilog(CRITICAL, "create_context: fail to allocate new memory");
        exit(1);
    }
    ctx->password = sdsnew(password);
    ctx->user_hash_table = NULL;
    ctx->connection_hash_table = NULL;
    ctx->channel_hash_table = NULL;
    ctx->irc_op_num = 0;
    pthread_mutex_init(&ctx->mutex_op_num, NULL);
    pthread_mutex_init(&ctx->mutex_user_table, NULL);
    pthread_mutex_init(&ctx->mutex_connection_table, NULL);
    pthread_mutex_init(&ctx->mutex_channel_table, NULL);
    return ctx;
}

void destroy_context(context_handle ctx) {
    if (ctx != NULL) {
        // TODO
    }
    free(ctx);
}

int increase_op_num(context_handle ctx) {
    if (ctx == NULL) {
        chilog(ERROR, "increase_op_num: empty params");
        return FAILURE;
    }
    pthread_mutex_lock(&ctx->mutex_op_num);
    ctx->irc_op_num += 1;
    pthread_mutex_unlock(&ctx->mutex_op_num);
    return SUCCESS;
}

int add_connection(context_handle ctx, connection_handle connection) {
    if (ctx == NULL || connection == NULL) {
        chilog(ERROR, "add_connection: empty params");
        return FAILURE;
    }
    pthread_mutex_lock(&ctx->mutex_connection_table);
    HASH_ADD_INT(ctx->connection_hash_table, socket_num, connection);
    pthread_mutex_unlock(&ctx->mutex_connection_table);
    return SUCCESS;
}

int modify_connection_state(context_handle ctx, int id, int state) {
    if (ctx == NULL) {
        chilog(ERROR, "modify_connection_state: empty params");
        return FAILURE;
    }

    connection_handle connection = NULL;
    pthread_mutex_lock(&ctx->mutex_connection_table);
    HASH_FIND_INT(ctx->connection_hash_table, &id, connection);
    pthread_mutex_unlock(&ctx->mutex_connection_table);

    if (!connection) {
        chilog(ERROR, "modify_connection_state: no such connection, id: %d", id);
        return FAILURE;
    }

    if(state != REGISTERED_CONNECTION && state != USER_CONNECTION) {
        chilog(ERROR, "unknown state for connection");
        return FAILURE;
    }

    if(connection->state != REGISTERED_CONNECTION) {
        connection->state = state;
    }

    return SUCCESS;
}

int delete_connection(context_handle ctx, int socket_num) {
    if (ctx == NULL) {
        chilog(ERROR, "delete_connection: empty params");
        return FAILURE;
    }
    pthread_mutex_lock(&ctx->mutex_connection_table);
    connection_handle connection = NULL;
    HASH_FIND_INT(ctx->connection_hash_table, &socket_num, connection);
    if (connection) {
        HASH_DEL(ctx->connection_hash_table, connection);
    }
    pthread_mutex_unlock(&ctx->mutex_connection_table);
    return SUCCESS;
}

int *count_connection_state(context_handle ctx) {
    if (ctx == NULL) {
        chilog(ERROR, "count_connection_state: empty params");
        return NULL;
    }
    int *res = calloc(3, sizeof(int));
    if (res == NULL) {
        chilog(CRITICAL, "count_connection_state: fail to allocate new memory");
        exit(1);
    }
    pthread_mutex_lock(&ctx->mutex_connection_table);
    for (connection_handle con = ctx->connection_hash_table; con != NULL; con = con->hh.next) {
        if (con->state == UNKNOWN_CONNECTION) {
            res[0] += 1;
        } else if (con->state == USER_CONNECTION) {
            res[1] += 1;
        } else {
            res[1] += 1;
            res[2] += 1;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_connection_table);
    return res;
}

int add_user_nick(context_handle ctx, char *nick, user_handle user) {
    chilog(INFO, "add user nick");
    if (ctx == NULL || nick == NULL || sdslen(nick) < 1 || user == NULL) {
        chilog(ERROR, "add_user_nick: empty params");
        return FAILURE;
    }
    user_handle temp = NULL;
    pthread_mutex_lock(&ctx->mutex_user_table);
    HASH_FIND_STR(ctx->user_hash_table, nick, temp);
    if (temp) {
        chilog(INFO, "nick %s already in use", nick);
        pthread_mutex_unlock(&ctx->mutex_user_table);
        return NICK_IN_USE;
    }
    user->nick = sdscpylen(sdsempty(), nick, sdslen(nick));
    HASH_ADD_KEYPTR(hh, ctx->user_hash_table, user->nick, sdslen(user->nick), user);
    pthread_mutex_unlock(&ctx->mutex_user_table);
    chilog(INFO, "successfully add user %s to context", user->nick);
    return SUCCESS;
}

int update_user_nick(context_handle ctx, char *nick, user_handle user, channel_handle **arr, int *count) {
    if (ctx == NULL || nick == NULL || sdslen(nick) < 1 || user == NULL) {
        chilog(ERROR, "update_user_nick: empty params");
        return FAILURE;
    }

    user_handle temp = NULL;
    pthread_mutex_lock(&ctx->mutex_user_table);
    HASH_FIND_STR(ctx->user_hash_table, nick, temp);
    if (temp) {
        chilog(INFO, "nick %s already in use", nick);
        pthread_mutex_unlock(&ctx->mutex_user_table);
        return NICK_IN_USE;
    }

    // update user nick in channel first
    *arr = update_nick_on_channel(ctx, nick, count);

    HASH_DEL(ctx->user_hash_table, user);
    user->nick = sdscpylen(sdsempty(), nick, sdslen(nick));
    HASH_ADD_KEYPTR(hh, ctx->user_hash_table, user->nick, sdslen(user->nick), user);
    pthread_mutex_unlock(&ctx->mutex_user_table);
    return SUCCESS;
}

user_handle get_user(context_handle ctx, char *nick) {
    if (ctx == NULL || nick == NULL || sdslen(nick) < 1) {
        chilog(ERROR, "get_user: empty params");
        return NULL;
    }
    user_handle user = NULL;
    pthread_mutex_lock(&ctx->mutex_user_table);
    HASH_FIND_STR(ctx->user_hash_table, nick, user);
    pthread_mutex_unlock(&ctx->mutex_user_table);
    return user;
}

int delete_user(context_handle ctx, user_handle user) {
    if (ctx == NULL || user == NULL) {
        chilog(ERROR, "delete_user: empty params");
        return FAILURE;
    }
    pthread_mutex_lock(&ctx->mutex_user_table);
    HASH_DEL(ctx->user_hash_table, user);
    pthread_mutex_unlock(&ctx->mutex_user_table);
    return SUCCESS;
}

int get_channel_count(context_handle ctx) {
    if (ctx == NULL) {
        chilog(ERROR, "get_channel_count: empty params");
        return -1;
    }
    pthread_mutex_lock(&ctx->mutex_channel_table);
    unsigned int count = HASH_COUNT(ctx->channel_hash_table);
    pthread_mutex_lock(&ctx->mutex_channel_table);
    return ((int) count);
}

channel_handle get_channel(context_handle ctx, char *name) {
    if (ctx == NULL || name == NULL || sdslen(name) < 1) {
        chilog(ERROR, "get_channel: empty params");
        return NULL;
    }
    channel_handle channel = NULL;
    pthread_mutex_lock(&ctx->mutex_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, name, channel);
    pthread_mutex_unlock(&ctx->mutex_channel_table);
    return channel;
}

channel_handle try_get_channel(context_handle ctx, char *name, bool *is_creator) {
    if (ctx == NULL || name == NULL || sdslen(name) < 1 || is_creator == NULL) {
        chilog(ERROR, "try_get_channel: empty params");
        return NULL;
    }
    channel_handle channel = NULL;
    pthread_mutex_lock(&ctx->mutex_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, name, channel);
    if (channel) {
        pthread_mutex_unlock(&ctx->mutex_channel_table);
        *is_creator = false;
        return channel;
    }
    // create a new channel
    channel = create_channel(name);
    *is_creator = true;
    HASH_ADD_KEYPTR(hh, ctx->channel_hash_table, channel->name, strlen(channel->name), channel);
    pthread_mutex_unlock(&ctx->mutex_channel_table);
    return channel;
}

channel_handle *get_channels_user_on(context_handle ctx, char *nick, int *count) {
    if (ctx == NULL || nick == NULL || sdslen(nick) < 1 || count == NULL) {
        chilog(ERROR, "get_channels_user_on: empty params");
        return NULL;
    }
    pthread_mutex_lock(&ctx->mutex_channel_table);
    unsigned int num = HASH_COUNT(ctx->channel_hash_table);
    channel_handle *arr = calloc(num, sizeof(channel_handle));
    if (arr == NULL) {
        chilog(CRITICAL, "get_channels_user_on: can't allocate new memory");
        exit(1);
    }

    int i = 0;
    for (channel_handle cha = ctx->channel_hash_table; cha != NULL; cha = cha->hh.next) {
        if (already_on_channel(cha, nick))
            arr[i++] = cha;
    }

    pthread_mutex_unlock(&ctx->mutex_channel_table);
    *count = i;
    return arr;
}

char **get_all_channel_names(context_handle ctx, int *count) {
    if (ctx == NULL || count == NULL) {
        chilog(ERROR, "get_all_channel_names: empty params");
        return NULL;
    }
    pthread_mutex_lock(&ctx->mutex_channel_table);
    unsigned int num = HASH_COUNT(ctx->channel_hash_table);

    char **arr = calloc(num, sizeof(char *));
    if (arr == NULL) {
        chilog(CRITICAL, "get_all_channel_names: can't allocate new memories");
        exit(1);
    }

    int i = 0;
    for (channel_handle cha = ctx->channel_hash_table; cha != NULL; cha = cha->hh.next) {
        arr[i++] = cha->name;
    }

    pthread_mutex_unlock(&ctx->mutex_channel_table);
    *count = num;
    return arr;
}

channel_handle *update_nick_on_channel(context_handle ctx, char *nick, int *count) {
    if (ctx == NULL || nick == NULL || sdslen(nick) < 1) {
        chilog(ERROR, "update_nick_on_channel: empty params");
        return NULL;
    }

    pthread_mutex_lock(&ctx->mutex_channel_table);
    unsigned int num = HASH_COUNT(ctx->channel_hash_table);
    channel_handle *arr = calloc(num, sizeof(channel_handle));

    int i = 0;
    for (channel_handle cha = ctx->channel_hash_table; cha != NULL; cha = cha->hh.next) {
        int rv = update_member_nick(cha, nick);
        if (rv == 0) {
            // find a channel that user is on
            chilog(INFO, "user %s on channel %s, need to notify members with new nick", nick, cha->name);
            arr[i++] = cha;
        } else if (rv == -1) {
            // error occurs
            pthread_mutex_unlock(&ctx->mutex_channel_table);
            free(arr);
            return NULL;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_channel_table);
    *count = i;
    return arr;
}