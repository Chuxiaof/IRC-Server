#include "channel.h"

#include "log.h"

channel_handle create_channel(char *name) {
    channel_handle channel = calloc(1, sizeof(channel_t));
    if (channel == NULL) {
        chilog(CRITICAL, "create_channel: fail to allocate memory");
        exit(1);
    }
    channel->name = name;
    channel->user_table = NULL;
    pthread_mutex_init(&channel->lock_channel_user, NULL);
    return channel;
}

void join_channel(channel_handle channel, user_handle user) {
    if (channel == NULL || user == NULL) {
        chilog(CRITICAL, "join_channel: empty params");
        return;
    }
    pthread_mutex_lock(&channel->lock_channel_user);
    HASH_ADD_KEYPTR(hh, channel->user_table, user->nick, strlen(user->nick), user);
    pthread_mutex_unlock(&channel->lock_channel_user);
    chilog(INFO, "user %s joined channel %s", user->nick, channel->name);
}

bool already_on_channel(channel_handle channel, char *nick) {
    if (channel == NULL || nick == NULL || strlen(nick) < 1) {
        chilog(CRITICAL, "already_on_channel: empty params");
        return false;
    }
    
    user_handle user = NULL;
    pthread_mutex_lock(&channel->lock_channel_user);
    HASH_FIND_STR(channel->user_table, nick, user);
    pthread_mutex_unlock(&channel->lock_channel_user);
    return user != NULL;
}

sds all_user_nicks(channel_handle channel) {
    if (channel == NULL) {
        chilog(CRITICAL, "all_user_nicks: empty params");
        return NULL;
    }
    pthread_mutex_lock(&channel->lock_channel_user);
    unsigned int count = HASH_COUNT(channel->user_table);
    char **res = malloc(count * sizeof(char *));
    int i = 0;
    for (user_handle usr = channel->user_table; usr != NULL; usr = usr->hh.next) {
        res[i] = usr->nick;
        i++;
    }
    pthread_mutex_unlock(&channel->lock_channel_user);
    sds rv = sdsjoin(res, count, " ");
    free(res);
    return rv;
}

void leave_channel(channel_handle channel, user_handle user) {
    if (channel == NULL || user == NULL) {
        chilog(CRITICAL, "leave_channel: empty params");
        return;
    }
    pthread_mutex_lock(&channel->lock_channel_user);
    HASH_DEL(channel->user_table, user);
    pthread_mutex_unlock(&channel->lock_channel_user);
    chilog(INFO, "delete %s from user table of channel %s", user->nick, channel->name);
}

unsigned int channel_user_count(channel_handle channel) {
    if (channel == NULL) {
        chilog(CRITICAL, "channel_user_count: empty params");
        return 0;
    }
    pthread_mutex_lock(&channel->lock_channel_user);
    unsigned int count = HASH_COUNT(channel->user_table);
    pthread_mutex_unlock(&channel->lock_channel_user);
    return count;
}

bool empty_channel(channel_handle channel) {
    if (channel == NULL) {
        chilog(CRITICAL, "empty_channel: empty params");
        return false;
    }
    return channel_user_count(channel) == 0;
}