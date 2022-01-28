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

#define SUFFICIENT 1
#define INSUFFICIENT 2
static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx);

#define REGISTERED 3
#define NOTREGISTERED 4
static int check_registered(context_handle ctx, user_handle user_info);

int send_reply(char *str, user_handle user_info, bool to_free);

static int send_welcome(user_handle user_info, char *server_host_name);

// send message(reply) to all the members in the channel except the sender itself
// if there's no need to exclude the sender, set sender_nick argument as NULL
int notify_all_channel_members(context_handle ctx, channel_handle channel, char *reply, char * sender_nick);

int handler_NICK(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (msg->nparams < 1) {
        // ERR_NONICKNAMEGIVEN
        chilog(ERROR, "handler_NICK: no nickname given");
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s :No nickname given\r\n",
                ctx->server_host, ERR_NONICKNAMEGIVEN, user_info->nick || "*");
        return send_reply(reply, user_info, true);
    }

    char *old_nick = user_info->nick;
    char *new_nick = msg->params[0];

    channel_handle *affected_channels;
    int affected_channel_count;

    int rv = user_info->registered ? update_user_nick(ctx, new_nick, user_info, &affected_channels, &affected_channel_count) 
                                    : add_user_nick(ctx, new_nick, user_info);

    switch (rv)
    {
    case NICK_IN_USE:
        chilog(INFO, "nick %s already in use", new_nick);
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :Nickname is already in use\r\n",
                ctx->server_host, ERR_NICKNAMEINUSE, user_info->nick || "*", new_nick);
        return send_reply(reply, user_info, true);
    case FAILURE:
        chilog(ERROR, "error occurs for handler_NICK");
        return FAILURE;
    default:    // success
        break;
    }

    if (user_info->registered) {
        if (affected_channel_count > 0) {
            sds reply = sdscatfmt(sdsempty(), ":%s!%s@%s NICK %s\r\n", 
                old_nick, user_info->username, user_info->client_host_name, new_nick);
            for (int i = 0; i < affected_channel_count; i++) {
                if (affected_channels[i] == NULL) {
                    chilog(WARNING, "handler_NICK: null channel");
                    continue;
                }

                notify_all_channel_members(ctx, affected_channels[i], reply, NULL);
            }
            sdsfree(reply);
            free(affected_channels);
        }
        return SUCCESS;
    } else if (can_register(user_info)) {
        user_info->registered = true;
        // labels this connection as a registered connection
        modify_connection_state(ctx, user_info->client_fd, REGISTERED_CONNECTION);
        send_welcome(user_info, ctx->server_host);
        handler_LUSERS(ctx, user_info, msg);
    } else {
        // labels this connection as a user connection
        modify_connection_state(ctx, user_info->client_fd, USER_CONNECTION);
    }

    return SUCCESS;
}

int handler_USER(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (user_info->registered) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s :Unauthorized command (already registered)\r\n", 
            ctx->server_host, ERR_ALREADYREGISTRED, user_info->nick);
        return send_reply(reply, user_info, true);
    }

    int ret = check_insufficient_param(msg->nparams, 4, "USER", user_info, ctx);
    if (ret == INSUFFICIENT) {
        chilog(INFO, "handler_USER: insufficient params");
        return SUCCESS;
    } else if (ret == FAILURE) {
        chilog(ERROR, "handler_USER: error in sending insufficient params reply");
        return FAILURE;
    }

    user_info->username = sdscpylen(sdsempty(), msg->params[0], sdslen(msg->params[0]));
    user_info->fullname = sdscpylen(sdsempty(), msg->params[msg->nparams - 1], sdslen(msg->params[msg->nparams - 1]));

    if (can_register(user_info)) {
        // add to ctx->user_table
        user_info->registered = true;

        // labels this connection as a registered connection
        modify_connection_state(ctx, user_info->client_fd, REGISTERED_CONNECTION);

        // send welcome
        send_welcome(user_info, ctx->server_host);
        handler_LUSERS(ctx, user_info, msg);
    } else {
        // labels this connection as a user connection
        modify_connection_state(ctx, user_info->client_fd, USER_CONNECTION);
    }

    return SUCCESS;
}

int handler_PRIVMSG(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    if (msg->nparams < 1) {
        // ERR_NORECIPIENT
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s :No recipient given (PRIVMSG)\r\n",
                ctx->server_host, ERR_NORECIPIENT, user_info->nick);
        return send_reply(reply, user_info, true);
    }

    if (!msg->longlast) {
        // ERR_NOTEXTTOSEND
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s :No text to send\r\n",
                ctx->server_host, ERR_NOTEXTTOSEND, user_info->nick);
        return send_reply(reply, user_info, true);
    }

    char *target_name = msg->params[0];
    user_handle target_user = NULL;
    channel_handle target_channel = NULL;
    bool is_channel = false;
    if (target_name[0] != '#') {
        // send to user
        target_user = get_user(ctx, target_name);
    } else {
        // send to channel
        is_channel = true;
        target_channel = get_channel(ctx, target_name);
    }

    if (target_user == NULL && target_channel == NULL) {
        // ERR_NOSUCHNICK
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :No such nick/channel\r\n",
                ctx->server_host, ERR_NOSUCHNICK, user_info->nick, target_name);
        return send_reply(reply, user_info, true);
    }

    // if the name is a nick, then send private message directly
    if (!is_channel) {
        sds reply = sdscatfmt(sdsempty(), ":%s!%s@%s PRIVMSG %s :%s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name,
                target_name, msg->params[msg->nparams - 1]);
        chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
        return send_reply(reply, target_user, true);
    }

    //if the name is a channel
    //firstly, check whether the sender is in this channel
    if(!already_on_channel(target_channel, user_info->nick)) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :Cannot send to channel\r\n",
                ctx->server_host, ERR_CANNOTSENDTOCHAN, user_info->nick, target_name);
        return send_reply(reply, user_info, true);
    }

    // send message to all channel members
    sds reply = sdscatfmt(sdsempty(), ":%s!%s@%s PRIVMSG %s :%s\r\n",
            user_info->nick, user_info->username, user_info->client_host_name,
            target_name, msg->params[msg->nparams - 1]);
    // notify all
    notify_all_channel_members(ctx, target_channel, reply, user_info->nick);
    sdsfree(reply);
    return SUCCESS;
}

int handler_NOTICE(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    if (!msg->longlast || msg->nparams < 2) {
        chilog(WARNING, "handler_NOTICE: error params");
        return SUCCESS;
    }

    char *target_name = msg->params[0];
    user_handle target_user = NULL;
    channel_handle target_channel = NULL;
    bool is_channel = false;
    if (target_name[0] != '#') {
        target_user = get_user(ctx, target_name);
    } else {
        is_channel = true;
        target_channel = get_channel(ctx, target_name);
    }

    if (target_user == NULL && target_channel == NULL) {
        chilog(WARNING, "handler_NOTICE: no such nick/channel");
        return SUCCESS;
    }

    // if the name is a nick, then send private message directly
    if (!is_channel) {
        sds reply = sdscatfmt(sdsempty(), ":%s!%s@%s NOTICE %s :%s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name,
                target_name, msg->params[msg->nparams - 1]);
        chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
        return send_reply(reply, target_user, true);
    }

    //if the name is a channel
    //firstly, check whether the sender is in this channel
    if(!already_on_channel(target_channel, user_info->nick)) {
        chilog(WARNING, "handler_NOTICE: sender not in channel");
        return SUCCESS;
    }

    // send message to all channel members
    sds reply = sdscatfmt(sdsempty(), ":%s!%s@%s NOTICE %s :%s\r\n",
            user_info->nick, user_info->username, user_info->client_host_name,
            target_name, msg->params[msg->nparams - 1]);
    // notify all
    notify_all_channel_members(ctx, target_channel, reply, user_info->nick);
    sdsfree(reply);
    return SUCCESS;
}

int handler_PING(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    sds reply = sdscatfmt(sdsempty(), "PONG %s\r\n", ctx->server_host);
    return send_reply(reply, user_info, true);
}

int handler_PONG(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    chilog(INFO, "receive PONG from %s", user_info->client_host_name);
    // do nothing
    return SUCCESS;
}

int handler_WHOIS(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    if (msg->nparams < 1) {
        // just ignore
        return SUCCESS;
    }

    char *target_nick = msg->params[0];

    user_handle target_user = get_user(ctx, target_nick);

    if (!target_user) {
        // ERR_NOSUCHNICK
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :No such nick/channel\r\n",
                ctx->server_host, ERR_NOSUCHNICK, user_info->nick, target_nick);
        return send_reply(reply, user_info, true);
    }

    sds r_whoisuser = sdscatfmt(sdsempty(), ":%s %s %s %s %s %s * :%s\r\n",
            ctx->server_host, RPL_WHOISUSER, user_info->nick,
            target_user->nick, target_user->username,
            target_user->client_host_name, target_user->fullname);

    if (send_reply(r_whoisuser, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_whoisserver = sdscatfmt(sdsempty(), ":%s %s %s %s %s :chirc-1.0\r\n",
            ctx->server_host, RPL_WHOISSERVER, user_info->nick,
            user_info->nick, ctx->server_host);

    if (send_reply(r_whoisserver, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_end = sdscatfmt(sdsempty(), ":%s %s %s %s :End of WHOIS list\r\n",
            ctx->server_host, RPL_ENDOFWHOIS, user_info->nick, user_info->nick);
    return send_reply(r_end, user_info, true);
}

int handler_UNKNOWNCOMMAND(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (!user_info->registered) {
        // just ignore
        return SUCCESS;
    }

    sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :Unknown command\r\n",
            ctx->server_host, ERR_UNKNOWNCOMMAND, user_info->nick, msg->cmd);
    return send_reply(reply, user_info, true);
}

int handler_QUIT(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    char *quit_msg;
    quit_msg = msg->longlast ? msg->params[msg->nparams - 1] : "Client Quit";

    // notify all
    // :syrk!kalt@millennium.stealth.net QUIT :Gone to have lunch
    channel_handle *affected_channel;
    int affected_channel_count;
    affected_channel = get_channels_user_on(ctx, user_info->nick, &affected_channel_count);
    if (affected_channel_count > 0) {
        sds r_channel = sdscatfmt(sdsempty(), ":%s!%s@%s QUIT :%s",
            user_info->nick, user_info->username, user_info->client_host_name, quit_msg);
        for (int i = 0; i < affected_channel_count; i++) {
            if (affected_channel[i] == NULL) {
                chilog(WARNING, "handler_QUIT: null channel");
                continue;
            }
            notify_all_channel_members(ctx, affected_channel[i], r_channel, user_info->nick);
        }
        sdsfree(r_channel);
    }
    
    sds reply = sdscatfmt(sdsempty(), "ERROR :Closing Link: %s (%s)\r\n",
            user_info->client_host_name, quit_msg);
    send_reply(reply, user_info, true);
    return FAILURE;
}

int handler_LUSERS(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    // get connections statistics
    int *count = count_connection_state(ctx);

    // TODO: change the parameters for op and channel
    sds r_luser_client = sdscatfmt(sdsempty(), ":%s %s %s :There are %d users and %d services on %d servers\r\n",
            ctx->server_host, RPL_LUSERCLIENT, user_info->nick, count[2], 0, 1);

    if (send_reply(r_luser_client, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_luser_op = sdscatfmt(sdsempty(), ":%s %s %s %d :operator(s) online\r\n",
            ctx->server_host, RPL_LUSEROP, user_info->nick, ctx->irc_op_num);

    if (send_reply(r_luser_op, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_luser_unknown = sdscatfmt(sdsempty(), ":%s %s %s %d :unknown connection(s)\r\n",
            ctx->server_host, RPL_LUSERUNKNOWN, user_info->nick, count[0]);

    if (send_reply(r_luser_unknown, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_luser_channels = sdscatfmt(sdsempty(), ":%s %s %s %d :channels formed\r\n",
            ctx->server_host, RPL_LUSERCHANNELS, user_info->nick, get_channel_count(ctx));

    if (send_reply(r_luser_channels, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_luser_me = sdscatfmt(sdsempty(), ":%s %s %s :I have %d clients and %d servers\r\n",
            ctx->server_host, RPL_LUSERME, user_info->nick, count[1], 1);

    if (send_reply(r_luser_me, user_info, true) == FAILURE) {
        return FAILURE;
    }

    free(count);

    sds r_nomotd = sdscatfmt(sdsempty(), ":%s %s %s :MOTD File is missing\r\n", 
        ctx->server_host, ERR_NOMOTD, user_info->nick);

    return send_reply(r_nomotd, user_info, true);
}

int handler_JOIN(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }
    
    ret = check_insufficient_param(msg->nparams, 1, "JOIN", user_info, ctx);
    if (ret != SUFFICIENT) {
        return ret;
    }

    char *name = msg->params[0];
    bool is_creator = false;
    channel_handle channel = try_get_channel(ctx, name, &is_creator);

    // add user to current channel
    int rv = join_channel(channel, user_info->nick, is_creator);

    // send reply
    switch (rv) {
    case 0:
        chilog(INFO, "user %s joined channel %s", user_info->nick, name);
        // notify all users :nick!user@10.150.42.58 JOIN #test
        sds r_join = sdscatfmt(sdsempty(), ":%s!%s@%s JOIN %s\r\n",
                user_info->nick, user_info->username, user_info->client_host_name, name);
        notify_all_channel_members(ctx, channel, r_join, NULL);
        sdsfree(r_join);
        break;
    case 1:
        chilog(INFO, "handler_JOIN: ignored, user %s already on channel %s", user_info->nick, channel->name);
        return SUCCESS;
    default:
        chilog(CRITICAL, "handler_JOIN: unanticipated error");
        return FAILURE;
    }

    // skip RPL_TOPIC
    // :frost 353 nick = test :@nick
    sds all_nicks = member_nicks_str(channel);
    sds r_name = sdscatfmt(sdsempty(), ":%s %s %s = %s :@%s\r\n",
            ctx->server_host, RPL_NAMREPLY, user_info->nick, name, all_nicks);
    if (send_reply(r_name, user_info, true) == FAILURE) {
        sdsfree(all_nicks);
        return FAILURE;
    }
    sdsfree(all_nicks);

    // :hostname 366 nick #foobar :End of NAMES list
    sds r_end = sdscatfmt(sdsempty(), ":%s %s %s %s :End of NAMES list\r\n",
            ctx->server_host, RPL_ENDOFNAMES, user_info->nick, name);
    return send_reply(r_end, user_info, true);
}

int handler_PART(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }
    
    ret = check_insufficient_param(msg->nparams, 1, "PART", user_info, ctx);
    if (ret != SUFFICIENT) {
        return ret;
    }

    char *channel_name = msg->params[0];
    channel_handle channel = NULL;
    pthread_mutex_lock(&ctx->mutex_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, channel_name, channel);

    if (!channel) {
        // ERR_NOSUCHCHANNEL
        pthread_mutex_unlock(&ctx->mutex_channel_table);
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :No such channel\r\n",
                ctx->server_host, ERR_NOSUCHCHANNEL, user_info->nick, channel_name);
        return send_reply(reply, user_info, true);
    }

    int rv = leave_channel(channel, user_info->nick);
    switch (rv) {
    case 1:
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :You're not on that channel\r\n",
                ctx->server_host, ERR_NOTONCHANNEL, user_info->nick, channel_name);
        return send_reply(reply, user_info, true);
    case 0:
    case 2:
        chilog(INFO, "handler_PART: notify all members: user %s leave channel %s", user_info->nick, channel_name);
        sds reply;
        if (msg->longlast)
            reply = sdscatfmt(sdsempty(), ":%s!%s@%s PART %s :%s\r\n",
                    user_info->nick, user_info->username, user_info->client_host_name, channel_name, msg->params[1]);
        else
            reply = sdscatfmt(sdsempty(), ":%s!%s@%s PART %s\r\n",
                    user_info->nick, user_info->username, user_info->client_host_name, channel_name);
        
        // notify myself
        if (send_reply(reply, user_info, false) == FAILURE) {
            sdsfree(reply);
            return FAILURE;
        }

        // notify remaining members
        if (rv == 0) {
            notify_all_channel_members(ctx, channel, reply, NULL);
        }

        sdsfree(reply);
        break;
    default:
        chilog(CRITICAL, "handler_PART: unanticipated error");
        return FAILURE;
    }

    if (rv == 2) {
        // delete this channel
        chilog(INFO, "delete channel %s from context", channel_name);
        HASH_DEL(ctx->channel_hash_table, channel);
    }

    pthread_mutex_unlock(&ctx->mutex_channel_table);
    return SUCCESS;
}

int handler_LIST(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }
    
    if (msg->nparams == 1) {
        char *name = msg->params[0];
        channel_handle channel = get_channel(ctx, name);
        if (channel) {
            sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s %u :\r\n",
                    ctx->server_host, RPL_LIST, user_info->nick, channel->name, channel_member_count(channel));
            if (send_reply(reply, user_info, true) == FAILURE)
                return FAILURE;
        }
    } else {
        pthread_mutex_lock(&ctx->mutex_channel_table);
        for (channel_handle cha = ctx->channel_hash_table; cha != NULL; cha = cha->hh.next) {
            sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s %u :\r\n",
                    ctx->server_host, RPL_LIST, user_info->nick, cha->name, channel_member_count(cha));
            if (send_reply(reply, user_info, true) == FAILURE) {
                pthread_mutex_unlock(&ctx->mutex_channel_table);
                return FAILURE;
            }
        }
        pthread_mutex_unlock(&ctx->mutex_channel_table);
    }

    sds r_end = sdscatfmt(sdsempty(), ":%s %s %s :End of LIST\r\n",
            ctx->server_host, RPL_LISTEND, user_info->nick);
    return send_reply(r_end, user_info, true);
}

int handler_OPER(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }
    
    ret = check_insufficient_param(msg->nparams, 2, "OPER", user_info, ctx);
    if (ret != SUFFICIENT) {
        return ret;
    }

    char *given_pw = msg->params[1];
    if (strcmp(ctx->password, given_pw) != 0) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s :Password incorrect\r\n", 
            ctx->server_host, ERR_PASSWDMISMATCH, user_info->nick);
        return send_reply(reply, user_info, true);
    }

    user_info->is_irc_operator = true;
    increase_op_num(ctx);
    
    sds reply = sdscatfmt(sdsempty(), ":%s %s %s :You are now an IRC operator\r\n", 
        ctx->server_host, RPL_YOUREOPER, user_info->nick);
    return send_reply(reply, user_info, true);
}

int handler_MODE(context_handle ctx, user_handle user_info, message_handle msg)
{
    int ret = check_registered(ctx, user_info);
    if (ret != REGISTERED) {
        return ret;
    }

    ret = check_insufficient_param(msg->nparams, 3, "MODE", user_info, ctx);
    if (ret != SUFFICIENT) {
        return ret;
    }
    
    char *channel_name = msg->params[0];
    channel_handle channel = get_channel(ctx, channel_name);

    if (!channel) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :No such channel\r\n", 
            ctx->server_host, ERR_NOSUCHCHANNEL, user_info->nick, channel_name);
        return send_reply(reply, user_info, true);
    }

    char *mode_name = msg->params[1];
    if (strcmp(mode_name, "-o") != 0 && strcmp(mode_name, "+o") != 0) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :is unknown mode char to me for %s\r\n", 
            ctx->server_host, ERR_UNKNOWNMODE, user_info->nick, mode_name, channel_name);
        return send_reply(reply, user_info, true);
    }

    char *target_nick = msg->params[2];
    if (!already_on_channel(channel, target_nick)) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s %s :They aren't on that channel\r\n",
            ctx->server_host, ERR_USERNOTINCHANNEL, user_info->nick, target_nick, channel_name);
        return send_reply(reply, user_info, true);
    }

    if (user_info->is_irc_operator || is_channel_operator(channel, user_info->nick)) {
        if(update_member_mode(channel, target_nick, mode_name) == -1) {
            return FAILURE;
        }
        sds reply = sdscatfmt(sdsempty(), ":%s!%s@%s MODE %s %s %s\r\n", 
            user_info->nick, user_info->username, user_info->client_host_name,channel_name, mode_name, target_nick);
        // notify all
        notify_all_channel_members(ctx, channel, reply, NULL);
        sdsfree(reply);
        return SUCCESS;
    } else {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :You're not channel operator\r\n", 
            ctx->server_host, ERR_CHANOPRIVSNEEDED, user_info->nick, channel_name);
        return send_reply(reply, user_info, true);
    }
}

static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx)
{
    if (have < target) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s %s :Not enough parameters\r\n",
                ctx->server_host, ERR_NEEDMOREPARAMS, user_info->nick, cmd);
        if (send_reply(reply, user_info, true) == FAILURE) {
            return FAILURE;
        }
        return INSUFFICIENT;
    }
    return SUFFICIENT;
}

static int check_registered(context_handle ctx, user_handle user_info)
{
    if (!user_info->registered) {
        sds reply = sdscatfmt(sdsempty(), ":%s %s %s :You have not registered\r\n", 
            ctx->server_host, ERR_NOTREGISTERED, user_info->nick || "*");
        if (send_reply(reply, user_info, true) == FAILURE)
            return FAILURE;
        return NOTREGISTERED;
    } 
    return REGISTERED;
}

static int send_welcome(user_handle user_info, char *server_host_name)
{
    sds r_welcome = sdscatfmt(sdsempty(), ":%s %s %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
            server_host_name, RPL_WELCOME, user_info->nick,
            user_info->nick, user_info->username, user_info->client_host_name);
    if (send_reply(r_welcome, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_yourhost = sdscatfmt(sdsempty(), ":%s %s %s :Your host is %s, running version 1.0\r\n",
            server_host_name, RPL_YOURHOST, user_info->nick, user_info->client_host_name);
    if (send_reply(r_yourhost, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_created = sdscatfmt(sdsempty(), ":%s %s %s :This server was created TBD\r\n",
            server_host_name, RPL_CREATED, user_info->nick);
    if (send_reply(r_created, user_info, true) == FAILURE) {
        return FAILURE;
    }

    sds r_myinfo = sdscatfmt(sdsempty(), ":%s %s %s %s 1.0 ao mtov\r\n",
            server_host_name, RPL_MYINFO, user_info->nick, server_host_name);
    if (send_reply(r_myinfo, user_info, true) == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}

int notify_all_channel_members(context_handle ctx, channel_handle channel, char *reply, char * sender_nick)
{
    int count = 0;
    char **member_nicks = member_nicks_arr(channel, &count);
    chilog(INFO, "number of channel mumber: %d", count);
    for (int i = 0; i < count; i++) {
        if(sender_nick != NULL && sdscmp(sender_nick, member_nicks[i]) == 0) {
            // skip sender
            continue;
        }

        user_handle usr = get_user(ctx, member_nicks[i]);
        if (!usr) {
            chilog(WARNING, "notify_all_channel_members: fail to get user %s", member_nicks[i]);
            continue;
        }
        if (send_reply(reply, usr, false) == FAILURE) {
            free(member_nicks);
            return FAILURE;
        }
    }
    free(member_nicks);
    return SUCCESS;
}

// This chunk of code is from Beej, thanks for Beej!
// Ensure that we send all we want to send succgessfully
static int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while (total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

int send_reply(char *str, user_handle user_info, bool to_free)
{
    if (str == NULL || user_info == NULL) {
        chilog(ERROR, "illegal input args for send_reply: empty params");
        return FAILURE;
    }

    int len = sdslen(str);
    if (sendall(user_info->client_fd, str, &len) == -1) {
        chilog(ERROR, "send error in sendall()!");
        sdsfree(str);
        return FAILURE;
    }

    if (to_free) {
        sdsfree(str);
    }
    return SUCCESS;
}
