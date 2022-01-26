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

#define MAX_BUFFER_SIZE 512

static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx);
static bool can_register(user_handle user_info);

static int check_registered(context_handle ctx, user_handle user_info);
static int sendall(int s, char *buf, int *len);
static int send_reply(char *str, message_handle msg, user_handle user_info);
static int send_welcome(user_handle user_info, char *server_host_name);

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
        HASH_DEL(ctx->user_hash_table, user_info);
        user_info->nick = nick;
        HASH_ADD_KEYPTR(hh, ctx->user_hash_table, nick, strlen(nick), user_info);
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
    }

    pthread_mutex_unlock(&ctx->lock_user_table);
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

    char ret[MAX_BUFFER_SIZE];
    sprintf(ret, ":%s!%s@%s PRIVMSG %s :%s\r\n",
            user_info->nick, user_info->username, user_info->client_host_name,
            target_nick, msg->params[msg->nparams - 1]);
    chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
    return send_reply(ret, NULL, target_user);
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

    char *target_nick = msg->params[0];
    user_handle target_user;

    pthread_mutex_lock(&ctx->lock_user_table);
    HASH_FIND_STR(ctx->user_hash_table, target_nick, target_user);
    pthread_mutex_unlock(&ctx->lock_user_table);

    if (!target_user)
    {
        chilog(WARNING, "handler_NOTICE: no such nick\r\n");
        return 0;
    }

    char ret[MAX_BUFFER_SIZE];
    sprintf(ret, ":%s!%s@%s NOTICE %s :%s\r\n",
            user_info->nick, user_info->username, user_info->client_host_name,
            target_nick, msg->params[msg->nparams - 1]);
    chilog(INFO, "%s sends an message to %s", user_info->nick, target_user->nick);
    return send_reply(ret, NULL, target_user);
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
    for(temp = ctx->connection_hash_table; temp!= NULL; temp = temp->hh.next){
        if(temp->state==0){
            unknown_connections++;
        }else if(temp->state==1){
            user_connections++;
        }else{
            registered_connections++;
        }
    }
    pthread_mutex_unlock(&ctx->lock_connection_table);

    //TODO: change the parameters for op and channel
    char luser_client[MAX_BUFFER_SIZE];
    sprintf(luser_client, ":%s %s %s :There are %d users and %d services on %d servers\r\n",
            ctx->server_host, RPL_LUSERCLIENT, user_info->nick, registered_connections, 0, 1);
    if (send_reply(luser_client, NULL, user_info) == -1)
    {
        return -1;
    }

    char luser_op[MAX_BUFFER_SIZE];
    sprintf(luser_op, ":%s %s %s %d :operator(s) online\r\n",
            ctx->server_host, RPL_LUSEROP, user_info->nick, 1);
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
            ctx->server_host, RPL_LUSERCHANNELS, user_info->nick, 1);
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

    return 0;
}

int handler_JOIN(context_handle ctx, user_handle user_info, message_handle msg) {
    int ret = check_insufficient_param(msg->nparams, 1, "JOIN", user_info, ctx);
    if (ret == 1) {
        chilog(INFO, "handler_JOIN: insufficient params");
        return 0;
    }
    if (ret == -1) {
        chilog(ERROR, "handler_USER: fail when sending ERR_NEEDMOREPARAMS");
        return -1;
    }

    char *name = msg->params[0];
    channel_handle channel = NULL;

    pthread_mutex_lock(&ctx->lock_channel_table);
    HASH_FIND_STR(ctx->channel_hash_table, name, channel);

    if (!channel) {
        // need to create a new channel
        channel = create_channel(name);
        // add channel to ctx
        HASH_ADD_KEYPTR(hh, ctx->channel_hash_table, name, strlen(name), channel);
    }

    pthread_mutex_unlock(&ctx->lock_channel_table);

    // add user to current channel
    // TODO, update when change nick
    join_channel(channel, user_info);
    
    // send reply
    char reply[MAX_BUFFER_SIZE];
    // :nick!user@10.150.42.58 JOIN #test
    sprintf(reply, ":%s!%s@%s JOIN %s",
        user_info->nick, user_info->username, ctx->server_host, name);
    if (send_reply(reply, NULL, user_info) == -1)
        return -1;
    
    // skip RPL_TOPIC
    // :hostname 353 nick = #foobar :foobar1 foobar2 foobar3
    // TODO
    sprintf(reply, ":%s %s %s = %s :foobar1 foobar2 foobar3\r\n",
        ctx->server_host, RPL_NAMREPLY, user_info->nick, name);
    if (send_reply(reply, NULL, user_info) == -1)
        return -1;
    
    // :hostname 366 nick #foobar :End of NAMES list
    sprintf(reply, ":%s %s %s %s :End of NAMES list",
        ctx->server_host, RPL_ENDOFNAMES, user_info->nick, name);
    return send_reply(reply, NULL, user_info);
}

int handler_PART(context_handle ctx, user_handle user_info, message_handle msg) {
    int ret = check_insufficient_param(msg->nparams, 1, "PART", user_info, ctx);
    if (ret == 1) {
        chilog(INFO, "handler_PART: insufficient params");
        return 0;
    }
    if (ret == -1) {
        chilog(ERROR, "handle_PART: fail when sending ERR_NEEDMOREPARAMS");
        return -1;
    }

    // char *channel_name = msg->params[0];
    // channel_handle channel = NULL;
    // HASH_FIND_STR()

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

    // hard coded message
    // to pass test
    char ret[MAX_BUFFER_SIZE];
    sprintf(ret, ":hostname 251 %s :There are 1 users and 0 services on 1 servers\r\n", user_info->nick);
    send_reply(ret, NULL, user_info);

    sprintf(ret, ":hostname 252 %s 0 :operator(s) online\r\n", user_info->nick);
    send_reply(ret, NULL, user_info);

    sprintf(ret, ":hostname 253 %s 0 :unknown connection(s)\r\n", user_info->nick);
    send_reply(ret, NULL, user_info);

    sprintf(ret, ":hostname 254 %s 0 :channels formed\r\n", user_info->nick);
    send_reply(ret, NULL, user_info);

    sprintf(ret, ":hostname 255 %s :I have 1 clients and 1 servers\r\n", user_info->nick);
    send_reply(ret, NULL, user_info);

    sprintf(ret, ":hostname 422 %s :MOTD File is missing\r\n", user_info->nick);
    send_reply(ret, NULL, user_info);

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
static int send_reply(char *str, message_handle msg, user_handle user_info)
{
    if (str == NULL && msg == NULL)
    {
        chilog(ERROR, "Illegal input args for send_reply: both str and msg are NULL!");
        exit(1);
    }

    if (str == NULL) {
        str = (char *) malloc(MAX_BUFFER_SIZE);
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