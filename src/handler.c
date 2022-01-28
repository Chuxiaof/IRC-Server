#include <sys/socket.h>
#include <sds.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#include "handler.h"
#include "user.h"
#include "context.h"
#include "message.h"
#include "log.h"
#include "reply.h"
#include "connection.h"
#include "channel.h"

#define MAX_BUFFER_SIZE 512

static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx);
static bool can_register(user_handle user_info);

static int check_registered(context_handle ctx, user_handle user_info);
static int sendall(int s, char *buf, int *len);
static int send_welcome(user_handle user_info, char *server_host_name);
static int change_nick_name(context_handle ctx, user_handle user_info, char * new_nick_name);
int send_reply(char *str, message_handle msg, user_handle user_info);

int handler_LUSERS(context_handle ctx, user_handle user_info, message_handle msg);

// For all the handlers, return -1 if there's an error, return 0 otherwise
int handler_NICK(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (msg->nparams < 1)
    {
        chilog(ERROR, "handler_NICK: no nickname given");
        // ERR_NONICKNAMEGIVEN
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s * :No nickname given\r\n",
                ctx->server_host, ERR_NONICKNAMEGIVEN);
        return send_reply(ret, NULL, user_info);
    }

    char *nick = msg->params[0];

    user_handle temp;
    pthread_mutex_lock(&ctx->lock_user_table);
    HASH_FIND_STR(ctx->user_hash_table, nick, temp);
    if (temp)
    {
        // ERR_NICKNAMEINUSE
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s * %s :Nickname is already in use\r\n",
                ctx->server_host, ERR_NICKNAMEINUSE, nick);
        pthread_mutex_unlock(&ctx->lock_user_table);
        return send_reply(ret, NULL, user_info);
    }

    // labels this connection as a user connection
    // ignore if it's already registered
    pthread_mutex_lock(&ctx->lock_connection_table);
    modify_connection(&(ctx->connection_hash_table), user_info->client_fd, USER_CONNECTION);
    pthread_mutex_unlock(&ctx->lock_connection_table);

    if (user_info->registered)
    {
        // change nick
        // TODO: change relevant hashtable
        if(change_nick_name(ctx, user_info, nick)==-1){
            pthread_mutex_unlock(&ctx->lock_user_table);
            return -1;
        }
        pthread_mutex_unlock(&ctx->lock_user_table);
        return 0;
    }

    user_info->nick = nick;
    if (can_register(user_info))
    {
        user_info->registered = true;
        // labels this connection as a registered connection
        pthread_mutex_lock(&ctx->lock_connection_table);
        modify_connection(&(ctx->connection_hash_table), user_info->client_fd, REGISTERED_CONNECTION);
        pthread_mutex_unlock(&ctx->lock_connection_table);
        HASH_ADD_KEYPTR(hh, ctx->user_hash_table, nick, strlen(nick), user_info);
        send_welcome(user_info, ctx->server_host);
        handler_LUSERS(ctx, user_info, msg);
    }

    pthread_mutex_unlock(&ctx->lock_user_table);
    return 0;
}

static int change_nick_name(context_handle ctx, user_handle user_info, char * new_nick_name){
    channel_handle temp_channel;
    //pthread_mutex_lock(&ctx->lock_channel_table);
    for(temp_channel=ctx->channel_hash_table; temp_channel!=NULL; temp_channel=temp_channel->hh.next){
        //go through each channel
        //if the user is in this channel, then update the nick in this channel's member table 
        chilog(INFO, "Looping channel......");
        if(already_on_channel(temp_channel, user_info->nick)){
            chilog(INFO, "user is in indeed in this channel: %s!", temp_channel->name);
            membership_handle member;
            //pthread_mutex_lock(&temp_channel->mutex_member_table);
            HASH_FIND_STR(temp_channel->member_table, user_info->nick, member);
            HASH_DEL(temp_channel->member_table, member);
            member->nick = new_nick_name;
            HASH_ADD_KEYPTR(hh, temp_channel->member_table, member->nick, strlen(member->nick), member);
            //pthread_mutex_unlock(&temp_channel->mutex_member_table);
            char message[MAX_BUFFER_SIZE];
            sprintf(message, ":%s!%s@%s NICK %s\r\n", user_info->nick, user_info->username, user_info->client_host_name, new_nick_name);
            if(send_to_channel_members(ctx, temp_channel, message, NULL)==-1){
                return -1;
            };
            chilog(INFO, "already sent messages to members in the channel!");
        }
        
    }
    //pthread_mutex_unlock(&ctx->lock_channel_table);
    //update the nick name in global user_hash_tablex
    HASH_DEL(ctx->user_hash_table, user_info);
    user_info->nick = new_nick_name;
    HASH_ADD_KEYPTR(hh, ctx->user_hash_table, user_info->nick, strlen(user_info->nick), user_info);
    return 0;
}

int handler_USER(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (user_info->registered)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s :Unauthorized command (already registered)\r\n", ctx->server_host, ERR_ALREADYREGISTRED, user_info->nick);
        return send_reply(error_msg, NULL, user_info);
    }

    // return value of check_insufficient_param()
    // 1: insufficient param and send reply message successfully
    // 0: sufficient or more param
    //-1: insufficient param but error in sending reply
    int ret = check_insufficient_param(msg->nparams, 4, "USER", user_info, ctx);
    if (ret == 1)
    {
        chilog(ERROR, "handler_USER: insufficient params");
        return 0;
    }
    else if (ret == -1)
    {
        chilog(ERROR, "handler_USER: error in sending insufficient params reply");
        return -1;
    }

    user_info->username = msg->params[0];
    user_info->fullname = msg->params[msg->nparams - 1];

    // labels this connection as a user connection
    pthread_mutex_lock(&ctx->lock_connection_table);
    modify_connection(&(ctx->connection_hash_table), user_info->client_fd, USER_CONNECTION);
    pthread_mutex_unlock(&ctx->lock_connection_table);

    if (can_register(user_info))
    {
        // add to ctx->user_table
        user_info->registered = true;
        pthread_mutex_lock(&ctx->lock_user_table);
        HASH_ADD_KEYPTR(hh, ctx->user_hash_table, user_info->nick, strlen(user_info->nick), user_info);
        pthread_mutex_unlock(&ctx->lock_user_table);

        // labels this connection as a registered connection
        pthread_mutex_lock(&ctx->lock_connection_table);
        modify_connection(&(ctx->connection_hash_table), user_info->client_fd, REGISTERED_CONNECTION);
        pthread_mutex_unlock(&ctx->lock_connection_table);
        // send welcome
        send_welcome(user_info, ctx->server_host);
        handler_LUSERS(ctx, user_info, msg);
    }
    return 0;
}

int handler_PRIVMSG(context_handle ctx, user_handle user_info, message_handle msg)
{
    int res = check_registered(ctx, user_info);
    if (res != 1)
    {
        return res;
    }

    if (msg->nparams < 1)
    {
        // ERR_NORECIPIENT
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s %s :No recipient given (PRIVMSG)\r\n",
                ctx->server_host, ERR_NORECIPIENT, user_info->nick);
        return send_reply(ret, NULL, user_info);
    }

    if (!msg->longlast)
    {
        // ERR_NOTEXTTOSEND
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s %s :No text to send\r\n",
                ctx->server_host, ERR_NOTEXTTOSEND, user_info->nick);
        return send_reply(ret, NULL, user_info);
    }

    char *target_name = msg->params[0];
    user_handle target_user;
    channel_handle target_channel;
    bool is_channel = false;
    if (target_name[0] != '#')
    {
        pthread_mutex_lock(&ctx->lock_user_table);
        HASH_FIND_STR(ctx->user_hash_table, target_name, target_user);
        pthread_mutex_unlock(&ctx->lock_user_table);
    }
    else
    {
        is_channel = true;
        pthread_mutex_lock(&ctx->lock_channel_table);
        HASH_FIND_STR(ctx->channel_hash_table, target_name, target_channel);
        pthread_mutex_unlock(&ctx->lock_channel_table);
    }

    if (target_user == NULL && target_channel == NULL)
    {
        // ERR_NOSUCHNICK
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s %s %s :No such nick/channel\r\n",
                ctx->server_host, ERR_NOSUCHNICK, user_info->nick, target_name);
        return send_reply(ret, NULL, user_info);
    }

    // if the name is a nick, then send private message directly
    if (!is_channel)
    {
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s!%s@%s PRIVMSG %s :%s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name,
                target_name, msg->params[msg->nparams - 1]);
        chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
        return send_reply(ret, NULL, target_user);
    }

    //if the name is a channel
    //firstly, check whether the sender is in this channel
    if(!already_on_channel(target_channel, user_info->nick)){
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s %s %s :Cannot send to channel\r\n",
                ctx->server_host, ERR_CANNOTSENDTOCHAN, user_info->nick, target_name);
        return send_reply(ret, NULL, user_info);
    }
    // send message to all channel members
    char channel_message[MAX_BUFFER_SIZE];
    sprintf(channel_message, ":%s!%s@%s PRIVMSG %s :%s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name,
                target_name, msg->params[msg->nparams - 1]);
    return send_to_channel_members(ctx, target_channel, channel_message, user_info->nick);
}

int handler_NOTICE(context_handle ctx, user_handle user_info, message_handle msg)
{
    int res = check_registered(ctx, user_info);
    if (res != 1)
    {
        return res;
    }

    if (!msg->longlast || msg->nparams < 2)
    {
        chilog(WARNING, "handler_NOTICE: error params");
        return 0;
    }

    // char *target_nick = msg->params[0];
    // user_handle target_user;

    // pthread_mutex_lock(&ctx->lock_user_table);
    // HASH_FIND_STR(ctx->user_hash_table, target_nick, target_user);
    // pthread_mutex_unlock(&ctx->lock_user_table);

    // if (!target_user)
    // {
    //     chilog(WARNING, "handler_NOTICE: no such nick\r\n");
    //     return 0;
    // }

    // char ret[MAX_BUFFER_SIZE];
    // sprintf(ret, ":%s!%s@%s NOTICE %s :%s\r\n",
    //         user_info->nick, user_info->username, user_info->client_host_name,
    //         target_nick, msg->params[msg->nparams - 1]);
    // chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
    // return send_reply(ret, NULL, target_user);


    char *target_name = msg->params[0];
    user_handle target_user;
    channel_handle target_channel;
    bool is_channel = false;
    if (target_name[0] != '#')
    {
        pthread_mutex_lock(&ctx->lock_user_table);
        HASH_FIND_STR(ctx->user_hash_table, target_name, target_user);
        pthread_mutex_unlock(&ctx->lock_user_table);
    }
    else
    {
        is_channel = true;
        pthread_mutex_lock(&ctx->lock_channel_table);
        HASH_FIND_STR(ctx->channel_hash_table, target_name, target_channel);
        pthread_mutex_unlock(&ctx->lock_channel_table);
    }

    if (target_user == NULL && target_channel == NULL)
    {
        chilog(WARNING, "handler_NOTICE: no such nick/channel \r\n");
        return 0;
    }

    // if the name is a nick, then send private message directly
    if (!is_channel)
    {
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s!%s@%s NOTICE %s :%s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name,
                target_name, msg->params[msg->nparams - 1]);
        chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
        return send_reply(ret, NULL, target_user);
    }

    //if the name is a channel
    //firstly, check whether the sender is in this channel
    if(!already_on_channel(target_channel, user_info->nick)){
        chilog(WARNING, "handler_NOTICE: sender not in channel \r\n");
        return 0;
    }
    // send message to all channel members
    char channel_message[MAX_BUFFER_SIZE];
    sprintf(channel_message, ":%s!%s@%s NOTICE %s :%s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name,
                target_name, msg->params[msg->nparams - 1]);
    return send_to_channel_members(ctx, target_channel, channel_message, user_info->nick);
}

int handler_PING(context_handle ctx, user_handle user_info, message_handle msg)
{
    int res = check_registered(ctx, user_info);
    if (res != 1)
    {
        return res;
    }

    char ret[MAX_BUFFER_SIZE];
    sprintf(ret, "PONG %s\r\n", ctx->server_host);
    return send_reply(ret, NULL, user_info);
}

int handler_PONG(context_handle ctx, user_handle user_info, message_handle msg)
{
    int res = check_registered(ctx, user_info);
    if (res != 1)
    {
        return res;
    }

    chilog(INFO, "receive PONG from %s", user_info->client_host_name);
    // do nothing
    return 0;
}

int handler_WHOIS(context_handle ctx, user_handle user_info, message_handle msg)
{
    int res = check_registered(ctx, user_info);
    if (res != 1)
    {
        return res;
    }

    if (msg->nparams < 1)
    {
        // just ignore
        return 0;
    }

    char *target_nick = msg->params[0];

    user_handle target_user;
    pthread_mutex_lock(&ctx->lock_user_table);
    HASH_FIND_STR(ctx->user_hash_table, target_nick, target_user);
    pthread_mutex_unlock(&ctx->lock_user_table);

    if (!target_user)
    {
        // ERR_NOSUCHNICK
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %s %s %s :No such nick/channel\r\n",
                ctx->server_host, ERR_NOSUCHNICK, user_info->nick, target_nick);
        return send_reply(ret, NULL, user_info);
    }

    char whoisuser[MAX_BUFFER_SIZE];
    sprintf(whoisuser, ":%s %s %s %s %s %s * :%s\r\n",
            ctx->server_host, RPL_WHOISUSER, user_info->nick,
            target_user->nick, target_user->username,
            target_user->client_host_name, target_user->fullname);

    if (send_reply(whoisuser, NULL, user_info) == -1)
    {
        return -1;
    }

    char whoisserver[MAX_BUFFER_SIZE];
    sprintf(whoisserver, ":%s %s %s %s %s :chirc-1.0\r\n",
            ctx->server_host, RPL_WHOISSERVER, user_info->nick,
            user_info->nick, ctx->server_host);

    if (send_reply(whoisserver, NULL, user_info) == -1)
    {
        return -1;
    }

    char end[MAX_BUFFER_SIZE];
    sprintf(end, ":%s %s %s %s :End of WHOIS list\r\n",
            ctx->server_host, RPL_ENDOFWHOIS, user_info->nick, user_info->nick);
    return send_reply(end, NULL, user_info);
}

int handler_UNKNOWNCOMMAND(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (!user_info->registered)
    {
        // just ignore
        return 0;
    }

    char ret[MAX_BUFFER_SIZE];
    sprintf(ret, ":%s %s %s %s :Unknown command\r\n",
            ctx->server_host, ERR_UNKNOWNCOMMAND, user_info->nick, msg->cmd);
    return send_reply(ret, NULL, user_info);
}

int handler_QUIT(context_handle ctx, user_handle user_info, message_handle msg)
{
    // check whether the user has been registered
    int ret = check_registered(ctx, user_info);
    if (ret != 1)
    {
        return ret;
    }

    char *quit_msg;
    quit_msg = msg->longlast ? msg->params[(msg->nparams) - 1] : "Client Quit";

    char response[MAX_BUFFER_SIZE];
    sprintf(response, "ERROR :Closing Link: %s (%s)\r\n",
            user_info->client_host_name, quit_msg);
    send_reply(response, NULL, user_info);
    return -1;
}

int handler_LUSERS(context_handle ctx, user_handle user_info, message_handle msg)
{
    // check whether the user has been registered
    int ret = check_registered(ctx, user_info);
    if (ret != 1)
    {
        return ret;
    }

    // get connections statistics for reply
    int unknown_connections = 0;
    int user_connections = 0;
    int registered_connections = 0;

    connection_handle temp;

    pthread_mutex_lock(&ctx->lock_connection_table);
    for (temp = ctx->connection_hash_table; temp != NULL; temp = temp->hh.next)
    {
        if (temp->state == 0)
        {
            unknown_connections++;
        }
        else if (temp->state == 1)
        {
            user_connections++;
        }
        else
        {
            registered_connections++;
            user_connections++;
        }
    }
    pthread_mutex_unlock(&ctx->lock_connection_table);

    // TODO: change the parameters for op and channel
    char luser_client[MAX_BUFFER_SIZE];
    sprintf(luser_client, ":%s %s %s :There are %d users and %d services on %d servers\r\n",
            ctx->server_host, RPL_LUSERCLIENT, user_info->nick, registered_connections, 0, 1);
    if (send_reply(luser_client, NULL, user_info) == -1)
    {
        return -1;
    }

    char luser_op[MAX_BUFFER_SIZE];
    sprintf(luser_op, ":%s %s %s %d :operator(s) online\r\n",
            ctx->server_host, RPL_LUSEROP, user_info->nick, ctx->irc_op_num);
    if (send_reply(luser_op, NULL, user_info) == -1)
    {
        return -1;
    }

    char luser_unknown[MAX_BUFFER_SIZE];
    sprintf(luser_unknown, ":%s %s %s %d :unknown connection(s)\r\n",
            ctx->server_host, RPL_LUSERUNKNOWN, user_info->nick, unknown_connections);
    if (send_reply(luser_unknown, NULL, user_info) == -1)
    {
        return -1;
    }

    char luser_channels[MAX_BUFFER_SIZE];
    sprintf(luser_channels, ":%s %s %s %d :channels formed\r\n",
            ctx->server_host, RPL_LUSERCHANNELS, user_info->nick, HASH_COUNT(ctx->channel_hash_table));
    if (send_reply(luser_channels, NULL, user_info) == -1)
    {
        return -1;
    }

    char luser_me[MAX_BUFFER_SIZE];
    sprintf(luser_me, ":%s %s %s :I have %d clients and %d servers\r\n",
            ctx->server_host, RPL_LUSERME, user_info->nick, user_connections, 0);
    if (send_reply(luser_me, NULL, user_info) == -1)
    {
        return -1;
    }

    char nomotd[MAX_BUFFER_SIZE];
    sprintf(nomotd, ":%s %s %s :MOTD File is missing\r\n", ctx->server_host, ERR_NOMOTD, user_info->nick);
    if (send_reply(nomotd, NULL, user_info) == -1)
    {
        return -1;
    }

    return 0;
}

int handler_JOIN(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_insufficient_param(msg->nparams, 1, "JOIN", user_info, ctx);
    if (ret == 1)
    {
        chilog(INFO, "handler_JOIN: insufficient params");
        return 0;
    }
    if (ret == -1)
    {
        chilog(ERROR, "handler_USER: fail when sending ERR_NEEDMOREPARAMS");
        return -1;
    }

    char *name = msg->params[0];
    channel_handle channel = NULL;
    bool is_creator = false;

    pthread_mutex_lock(&ctx->lock_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, name, channel);

    if (!channel)
    {
        // need to create a new channel
        channel = create_channel(name);
        // add channel to ctx
        HASH_ADD_KEYPTR(hh, ctx->channel_hash_table, name, strlen(name), channel);
        chilog(INFO, "successfully created new channel %s", name);
        is_creator = true;
    }
    pthread_mutex_unlock(&ctx->lock_channel_table);

    // add user to current channel
    // TODO, update when change nick
    int rv = join_channel(channel, user_info->nick, is_creator);

    // send reply
    char reply[MAX_BUFFER_SIZE];
    switch (rv)
    {
    case 0:
        chilog(INFO, "user %s joined channel %s", user_info->nick, name);
        // notify all users :nick!user@10.150.42.58 JOIN #test
        sprintf(reply, ":%s!%s@%s JOIN %s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name, name);

        // int count = 0;
        // char **member_nicks = member_nicks_arr(channel, &count);
        // for (int i = 0; i < count; i++)
        // {
        //     // TODO mutex, extract common functions
        //     user_handle usr = NULL;
        //     HASH_FIND_STR(ctx->user_hash_table, member_nicks[i], usr);
        //     if (usr && send_reply(reply, NULL, usr) == -1)
        //     {
        //         free(member_nicks);
        //         return -1;
        //     }
        // }
        // free(member_nicks);
        if(send_to_channel_members(ctx, channel, reply, NULL)==-1){
            return -1;
        }
        break;
    case 1:
        chilog(INFO, "handler_JOIN: ignored, user %s already on channel %s", user_info->nick, channel->name);
        return 0;
    default:
        chilog(CRITICAL, "handler_JOIN: unanticipated error");
        return -1;
    }

    // skip RPL_TOPIC
    // :frost 353 nick = test :@nick
    sds all_nicks = member_nicks_str(channel);
    sprintf(reply, ":%s %s %s = %s :@%s\r\n",
            ctx->server_host, RPL_NAMREPLY, user_info->nick, name, all_nicks);
    if (send_reply(reply, NULL, user_info) == -1)
    {
        sdsfree(all_nicks);
        return -1;
    }
    sdsfree(all_nicks);

    // :hostname 366 nick #foobar :End of NAMES list
    sprintf(reply, ":%s %s %s %s :End of NAMES list\r\n",
            ctx->server_host, RPL_ENDOFNAMES, user_info->nick, name);
    return send_reply(reply, NULL, user_info);
}

int handler_PART(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_insufficient_param(msg->nparams, 1, "PART", user_info, ctx);
    if (ret == 1)
    {
        chilog(INFO, "handler_PART: insufficient params");
        return 0;
    }
    if (ret == -1)
    {
        chilog(ERROR, "handle_PART: fail when sending ERR_NEEDMOREPARAMS");
        return -1;
    }

    char *channel_name = msg->params[0];
    channel_handle channel = NULL;

    pthread_mutex_lock(&ctx->lock_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, channel_name, channel);
    pthread_mutex_unlock(&ctx->lock_channel_table);

    char reply[MAX_BUFFER_SIZE];
    if (!channel)
    {
        // ERR_NOSUCHCHANNEL
        sprintf(reply, ":%s %s %s %s :No such channel\r\n",
                ctx->server_host, ERR_NOSUCHCHANNEL, user_info->nick, channel_name);
        return send_reply(reply, NULL, user_info);
    }

    int rv = leave_channel(channel, user_info->nick);
    switch (rv)
    {
    case 1:
        sprintf(reply, ":%s %s %s %s :You're not on that channel\r\n",
                ctx->server_host, ERR_NOTONCHANNEL, user_info->nick, channel_name);
        return send_reply(reply, NULL, user_info);
    case 0:
    case 2:
        chilog(INFO, "handler_PART: notify all members: user %s leave channel %s", user_info->nick, channel_name);
        if (msg->longlast)
            sprintf(reply, ":%s!%s@%s PART %s :%s\r\n",
                    user_info->nick, user_info->username, user_info->client_host_name, channel_name, msg->params[1]);
        else
            sprintf(reply, ":%s!%s@%s PART %s\r\n",
                    user_info->nick, user_info->username, user_info->client_host_name, channel_name);
        // notify myself
        if (send_reply(reply, NULL, user_info) == -1)
            return -1;
        // notify remaining members
        int count = 0;
        char **member_nicks = member_nicks_arr(channel, &count);
        for (int i = 0; i < count; i++)
        {
            // TODO mutex, extract common functions
            user_handle usr = NULL;
            HASH_FIND_STR(ctx->user_hash_table, member_nicks[i], usr);
            if (usr && send_reply(reply, NULL, usr) == -1)
            {
                free(member_nicks);
                return -1;
            }
        }
        free(member_nicks);
        break;
    default:
        chilog(CRITICAL, "handler_PART: unanticipated error");
        return -1;
    }

    if (rv == 2)
    {
        // delete this channel
        chilog(INFO, "delete channel %s from context", channel_name);
        pthread_mutex_lock(&ctx->lock_channel_table);
        HASH_DEL(ctx->channel_hash_table, channel);
        pthread_mutex_unlock(&ctx->lock_channel_table);
    }

    return 0;
}

int handler_LIST(context_handle ctx, user_handle user_info, message_handle msg)
{
    char reply[MAX_BUFFER_SIZE];

    if (msg->nparams == 1)
    {
        char *name = msg->params[0];
        channel_handle channel = NULL;
        pthread_mutex_lock(&ctx->lock_channel_table);
        HASH_FIND_STR(ctx->channel_hash_table, name, channel);
        pthread_mutex_unlock(&ctx->lock_channel_table);
        if (channel)
        {
            sprintf(reply, ":%s %s %s %s %u :\r\n",
                    ctx->server_host, RPL_LIST, user_info->nick, channel->name, channel_member_count(channel));
            if (send_reply(reply, NULL, user_info) == -1)
                return -1;
        }
    }
    else
    {
        pthread_mutex_lock(&ctx->lock_channel_table);
        for (channel_handle cha = ctx->channel_hash_table; cha != NULL; cha = cha->hh.next)
        {
            sprintf(reply, ":%s %s %s %s %u :\r\n",
                    ctx->server_host, RPL_LIST, user_info->nick, cha->name, channel_member_count(cha));
            if (send_reply(reply, NULL, user_info) == -1)
            {
                pthread_mutex_unlock(&ctx->lock_channel_table);
                return -1;
            }
        }
        pthread_mutex_unlock(&ctx->lock_channel_table);
    }

    sprintf(reply, ":%s %s %s :End of LIST\r\n",
            ctx->server_host, RPL_LISTEND, user_info->nick);
    return send_reply(reply, NULL, user_info);
}

int handler_OPER(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_insufficient_param(msg->nparams, 2, "OPER", user_info, ctx);
    if (ret == 1)
    {
        chilog(INFO, "handler_JOIN: insufficient params");
        return 0;
    }
    if (ret == -1)
    {
        chilog(ERROR, "handler_USER: fail when sending ERR_NEEDMOREPARAMS");
        return -1;
    }

    char *given_pw = msg->params[1];
    if (strcmp(ctx->password, given_pw) != 0)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s :Password incorrect\r\n", ctx->server_host, ERR_PASSWDMISMATCH, user_info->nick);
        return send_reply(error_msg, NULL, user_info);
    }

    user_info->is_irc_operator = true;
    ctx->irc_op_num++;
    char reply[MAX_BUFFER_SIZE];
    sprintf(reply, ":%s %s %s :You are now an IRC operator\r\n", ctx->server_host, RPL_YOUREOPER, user_info->nick);
    return send_reply(reply, NULL, user_info);
}

int handler_MODE(context_handle ctx, user_handle user_info, message_handle msg)
{
    channel_handle channel = NULL;
    char *channel_name = msg->params[0];
    pthread_mutex_lock(&ctx->lock_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, channel_name, channel);
    pthread_mutex_unlock(&ctx->lock_channel_table);

    if (!channel)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s %s :No such channel\r\n", ctx->server_host, ERR_NOSUCHCHANNEL, user_info->nick, channel_name);
        return send_reply(error_msg, NULL, user_info);
    }

    char *mode_name = msg->params[1];
    if (strcmp(mode_name, "-o") != 0 && strcmp(mode_name, "+o") != 0)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s %s :is unknown mode char to me for %s\r\n", ctx->server_host, ERR_UNKNOWNMODE, user_info->nick, mode_name, channel_name);
        return send_reply(error_msg, NULL, user_info);
    }

    char *target_nick = msg->params[2];
    if (!already_on_channel(channel, target_nick))
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s %s %s :They aren't on that channel\r\n", ctx->server_host, ERR_USERNOTINCHANNEL, user_info->nick, target_nick, channel_name);
        return send_reply(error_msg, NULL, user_info);
    }

    if (is_channel_operator(channel, user_info->nick) || user_info->is_irc_operator)
    {   
        if(update_member_mode(channel, target_nick, mode_name)==-1){
            return -1;
        }
        char mode_msg[MAX_BUFFER_SIZE];
        sprintf(mode_msg, ":%s!%s@%s MODE %s %s %s\r\n", user_info->nick, user_info->username, user_info->client_host_name,channel_name, mode_name, target_nick);
        return send_to_channel_members(ctx, channel, mode_msg, NULL);
    }
    else
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s %s :You're not channel operator\r\n", ctx->server_host, ERR_CHANOPRIVSNEEDED, user_info->nick, channel_name);
        return send_reply(error_msg, NULL, user_info);
    }

    return 0;
}

static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx)
{
    if (have < target)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s %s :Not enough parameters\r\n",
                ctx->server_host, ERR_NEEDMOREPARAMS, user_info->nick, cmd);
        if (send_reply(error_msg, NULL, user_info) == -1)
        {
            return -1;
        }
        return 1;
    }
    return 0;
}

static bool can_register(user_handle user_info)
{
    return user_info->nick != NULL && user_info->username != NULL;
}

static int send_welcome(user_handle user_info, char *server_host_name)
{
    char welcome[MAX_BUFFER_SIZE];
    sprintf(welcome, ":%s %s %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
            server_host_name, RPL_WELCOME, user_info->nick,
            user_info->nick, user_info->username, user_info->client_host_name);
    if (send_reply(welcome, NULL, user_info) == -1)
    {
        return -1;
    }

    char yourhost[MAX_BUFFER_SIZE];
    sprintf(yourhost, ":%s %s %s :Your host is %s, running version 1.0\r\n",
            server_host_name, RPL_YOURHOST, user_info->nick, user_info->client_host_name);
    if (send_reply(yourhost, NULL, user_info) == -1)
    {
        return -1;
    }

    char created[MAX_BUFFER_SIZE];
    sprintf(created, ":%s %s %s :This server was created TBD\r\n",
            server_host_name, RPL_CREATED, user_info->nick);
    if (send_reply(created, NULL, user_info) == -1)
    {
        return -1;
    }

    char myinfo[MAX_BUFFER_SIZE];
    sprintf(myinfo, ":%s %s %s %s 1.0 ao mtov\r\n",
            server_host_name, RPL_MYINFO, user_info->nick, server_host_name);
    if (send_reply(myinfo, NULL, user_info) == -1)
    {
        return -1;
    }

    return 0;
}

// This chunk of code is from Beej, thanks for Beej!
// Ensure that we send all we want to send succgessfully
static int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while (total < *len)
    {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

// return -1 if there is an error, return 0 otherwise
int send_reply(char *str, message_handle msg, user_handle user_info)
{
    if (str == NULL && msg == NULL)
    {
        chilog(ERROR, "Illegal input args for send_reply: both str and msg are NULL!");
        exit(1);
    }

    if(user_info == NULL){
        chilog(ERROR, "Illegal input args for send_reply: user_info is NULL!");
        exit(1);
    }

    if (str == NULL)
    {
        str = calloc(1, MAX_BUFFER_SIZE);
        message_to_string(msg, str);
    }

    int len = strlen(str);
    if (sendall(user_info->client_fd, str, &len) == -1)
    {
        chilog(ERROR, "send error in sendall()!");
        return -1;
    }

    return 0;
}

// if already registered, return 1
// if not registered && send reply successffully, return 0
// if not registered && error in sending reply, return -1
static int check_registered(context_handle ctx, user_handle user_info)
{
    if (user_info->registered == false)
    {
        char *nick = user_info->nick ? user_info->nick : "*";
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %s %s :You have not registered\r\n", ctx->server_host, ERR_NOTREGISTERED, nick);
        return send_reply(error_msg, NULL, user_info);
    }
    else
    {
        return 1;
    }
}
