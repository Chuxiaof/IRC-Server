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
    return channel;
}

void join_channel(channel_handle channel, user_handle user) {
    if (channel == NULL || user == NULL) {
        chilog(CRITICAL, "join_channel: empty params");
        return;
    }
    HASH_ADD_KEYPTR(hh, channel->user_table, user->nick, strlen(user->nick), user);
}