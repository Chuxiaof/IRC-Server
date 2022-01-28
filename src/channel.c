#include "channel.h"
#include "log.h"
#include "handler.h"
#include "context.h"

channel_handle create_channel(char *name)
{
    channel_handle channel = calloc(1, sizeof(channel_t));
    if (channel == NULL) {
        chilog(CRITICAL, "create_channel: fail to allocate memory");
        exit(1);
    }
    channel->name = sdscpylen(sdsempty(), name, sdslen(name));
    channel->member_table = NULL;
    pthread_mutex_init(&channel->mutex_member_table, NULL);
    chilog(INFO, "create_channel: successfully created channel %s", name);
    return channel;
}

void destroy_channel(channel_handle channel)
{
    if (channel != NULL) {
        // TODO
    }
    free(channel);
}

bool already_on_channel(channel_handle channel, char *nick)
{
    if (channel == NULL || nick == NULL || strlen(nick) < 1) {
        chilog(CRITICAL, "already_on_channel: empty params");
        return false;
    }

    membership_handle member = NULL;
    pthread_mutex_lock(&channel->mutex_member_table);
    HASH_FIND_STR(channel->member_table, nick, member);
    pthread_mutex_unlock(&channel->mutex_member_table);

    if (member) {
        chilog(INFO, "user %s already on channel %s", nick, channel->name);
        return true;
    }
    chilog(INFO, "user %s not on channel %s", nick, channel->name);
    return false;
}

int join_channel(channel_handle channel, char *nick, bool is_creator)
{
    if (channel == NULL || nick == NULL || strlen(nick) < 1) {
        chilog(CRITICAL, "join_channel: empty params");
        return -1;
    }

    membership_handle member = NULL;
    pthread_mutex_lock(&channel->mutex_member_table);
    HASH_FIND_STR(channel->member_table, nick, member);

    if (member) {
        pthread_mutex_unlock(&channel->mutex_member_table);
        chilog(INFO, "join_channel: user %s already on channel %s", nick, channel->name);
        return 1;
    }

    member = calloc(1, sizeof(membership_t));
    if (member == NULL) {
        chilog(CRITICAL, "join_channel: fail to allocate memory for member_handle");
        exit(1);
    }

    member->nick = nick;
    member->is_channel_operator = is_creator;
    HASH_ADD_KEYPTR(hh, channel->member_table, nick, strlen(nick), member);
    pthread_mutex_unlock(&channel->mutex_member_table);
    chilog(INFO, "join_channel: successfully add user %s to channel %s", nick, channel->name);
    return 0;
}

int leave_channel(channel_handle channel, char *nick)
{
    if (channel == NULL || nick == NULL || strlen(nick) < 1) {
        chilog(CRITICAL, "leave_channel: empty params");
        return -1;
    }

    membership_handle member = NULL;
    pthread_mutex_lock(&channel->mutex_member_table);
    HASH_FIND_STR(channel->member_table, nick, member);

    if (!member) {
        pthread_mutex_unlock(&channel->mutex_member_table);
        chilog(INFO, "leave_channel: user %s not on channel %s", nick, channel->name);
        return 1;
    }

    HASH_DEL(channel->member_table, member);
    chilog(INFO, "leave_channel: successfully remove user %s from channel %s", nick, channel->name);

    unsigned int count = HASH_COUNT(channel->member_table);
    pthread_mutex_unlock(&channel->mutex_member_table);
    chilog(INFO, "leave_channel: current %u users on channel %s", count, channel->name);

    return count == 0 ? 2 : 0;
}

int update_member_nick(channel_handle channel, char *old_nick, char *new_nick) {
    if (channel == NULL || old_nick == NULL || sdslen(old_nick) < 1 || new_nick == NULL || sdslen(new_nick) < 1) {
        chilog(ERROR, "update_member_nick: empty params");
        return -1;
    }

    membership_handle member = NULL;
    pthread_mutex_lock(&channel->mutex_member_table);
    HASH_FIND_STR(channel->member_table, old_nick, member);
    if (!member) {
        pthread_mutex_unlock(&channel->mutex_member_table);
        return 1;
    }
    HASH_DEL(channel->member_table, member);
    member->nick = sdscpylen(sdsempty(), new_nick, sdslen(new_nick));
    HASH_ADD_KEYPTR(hh, channel->member_table, member->nick, sdslen(member->nick), member);
    pthread_mutex_unlock(&channel->mutex_member_table);
    return 0;
}

int channel_member_count(channel_handle channel)
{
    if (channel == NULL) {
        chilog(CRITICAL, "channel_user_count: empty params");
        return -1;
    }

    pthread_mutex_lock(&channel->mutex_member_table);
    unsigned int count = HASH_COUNT(channel->member_table);
    pthread_mutex_unlock(&channel->mutex_member_table);
    chilog(INFO, "channel_member_count: currently %u users on channel %s", count, channel->name);

    return ((int)count);
}

char **member_nicks_arr(channel_handle channel, int *count)
{
    if (channel == NULL) {
        chilog(CRITICAL, "member_nicks_arr: empty params");
        return NULL;
    }

    pthread_mutex_lock(&channel->mutex_member_table);
    unsigned int num = HASH_COUNT(channel->member_table);

    char **arr = calloc(num, sizeof(char *));
    if (arr == NULL) {
        chilog(CRITICAL, "all_user_nicks: fail to allocate new memory");
        exit(1);
    }

    int i = 0;
    for (membership_handle meb = channel->member_table; meb != NULL; meb = meb->hh.next) {
        arr[i] = meb->nick;
        i++;
    }
    pthread_mutex_unlock(&channel->mutex_member_table);

    chilog(INFO, "member_nicks_arr: successfully get all user nicks on channel %s", channel->name);
    *count = num;
    return arr;
}

sds member_nicks_str(channel_handle channel)
{
    if (channel == NULL) {
        chilog(CRITICAL, "member_nicks_str: empty params");
        return NULL;
    }

    int count = 0;
    char **arr = member_nicks_arr(channel, &count);
    sds rv = sdsjoin(arr, count, " ");
    free(arr);

    chilog(INFO, "member_nicks_str: successfully get all user nicks on channel %s", channel->name);
    return rv;
}

bool is_channel_operator(channel_handle channel, char *nick)
{
    if (channel == NULL || nick == NULL || strlen(nick) < 1) {
        chilog(CRITICAL, "is_channel_operator: empty params");
        return false;
    }

    membership_handle member = NULL;
    pthread_mutex_lock(&channel->mutex_member_table);
    HASH_FIND_STR(channel->member_table, nick, member);
    pthread_mutex_unlock(&channel->mutex_member_table);

    return (member ? member->is_channel_operator : false);
}

int update_member_mode(channel_handle channel, char *nick, char *mode)
{
    // mode: +o, -o
    if (channel == NULL || nick == NULL || strlen(nick) < 1 || mode == NULL || strlen(nick) < 1) {
        chilog(CRITICAL, "update_member_mode: empty params");
        return -1;
    }

    membership_handle member = NULL;
    pthread_mutex_lock(&channel->mutex_member_table);
    HASH_FIND_STR(channel->member_table, nick, member);
    pthread_mutex_unlock(&channel->mutex_member_table);

    if (!member) {
        chilog(INFO, "update_member_mode: user %s not on channel %s", nick, channel->name);
        return 1;
    }

    if (strcmp(mode, "+o") == 0) {
        member->is_channel_operator = true;
        chilog(INFO, "update_member_mode: give user %s operator privilige on channel %s", nick, channel->name);
        return 0;
    }

    if (strcmp(mode, "-o") == 0) {
        member->is_channel_operator = false;
        chilog(INFO, "update_member_mode: remove user %s operator privilige on channel %s", nick, channel->name);
        return 0;
    }

    chilog(INFO, "update_member_mode: unknown mode %s", mode);
    return 2;
}

