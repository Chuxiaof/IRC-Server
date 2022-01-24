#include <sys/socket.h>
#include <sds.h>
#include <stdbool.h>
#include <stdio.h>
#include "handler.h"
#include "user.h"
#include "context.h"
#include "message.h"
#include "log.h"
#include "reply.h"

#define MAX_BUFFER_SIZE 512

static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx);
static bool can_register(user_handle user_info);

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
        sprintf(ret, ":%s %d * :No nickname given\r\n",
                ctx->server_host, ERR_NONICKNAMEGIVEN);
        return send_reply(ret, NULL, user_info);
    }

    char *nick = msg->params[0];

    user_handle temp;
    HASH_FIND_STR(ctx->user_hash_table, nick, temp);
    if (temp)
    {
        // ERR_NICKNAMEINUSE
        char ret[MAX_BUFFER_SIZE];
        sprintf(ret, ":%s %d * %s :Nickname is already in use\r\n",
                ctx->server_host, ERR_NICKNAMEINUSE, nick);
        return send_reply(ret, NULL, user_info);
    }

    if (user_info->registered)
    {
        // change nick
        HASH_DEL(ctx->user_hash_table, user_info);
        user_info->nick = nick;
        HASH_ADD_KEYPTR(hh, ctx->user_hash_table, nick, strlen(nick), user_info);
        return 0;
    }

    user_info->nick = nick;
    if (can_register(user_info))
    {
        user_info->registered = true;
        HASH_ADD_KEYPTR(hh, ctx->user_hash_table, nick, strlen(nick), user_info);
        send_welcome(user_info, ctx->server_host);
    }

    return 0;
}

int handler_USER(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (user_info->registered == true)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %d %s :Unauthorized command (already registered)\r\n", ctx->server_host, ERR_ALREADYREGISTRED, user_info->nick);
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
    else if(ret == -1)
    {
        chilog(ERROR, "handler_USER: error in sending insufficient params reply");
        return -1;
    }

    user_info->username = msg->params[0];
    user_info->fullname = msg->params[3];
    if (can_register(user_info))
    {
        // add to ctx->user_table
        user_info->registered = true;
        HASH_ADD_KEYPTR(hh, ctx->user_hash_table, user_info->nick, strlen(user_info->nick), user_info);
        // send welcome
        send_welcome(user_info, ctx->server_host);
    }
    return 0;
}

static int check_insufficient_param(int have, int target, char *cmd, user_handle user_info, context_handle ctx)
{
    if (have < target)
    {
        char error_msg[MAX_BUFFER_SIZE];
        sprintf(error_msg, ":%s %d %s %s :Not enough parameters\r\n",
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
    sprintf(welcome, ":%s %d %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
            server_host_name, RPL_WELCOME, user_info->nick,
            user_info->nick, user_info->username, user_info->client_host_name);
    if (send_reply(welcome, NULL, user_info) == -1)
    {
        return -1;
    }

    char yourhost[MAX_BUFFER_SIZE];
    sprintf(yourhost, ":%s %d %s :Your host is %s, running version 1.0\r\n",
            server_host_name, RPL_YOURHOST, user_info->nick, user_info->client_host_name);
    if (send_reply(yourhost, NULL, user_info) == -1)
    {
        return -1;
    }

    char created[MAX_BUFFER_SIZE];
    sprintf(created, ":%s %d %s :This server was created TBD\r\n",
            server_host_name, RPL_CREATED, user_info->nick);
    if (send_reply(created, NULL, user_info) == -1)
    {
        return -1;
    }

    char myinfo[MAX_BUFFER_SIZE];
    sprintf(myinfo, ":%s %d %s %s TBD TBD TBD\r\n",
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

static int send_reply(char *str, message_handle msg, user_handle user_info)
{
    if (str == NULL && msg == NULL)
    {
        chilog(ERROR, "Illegal input args for send_reply: both str and msg are NULL!");
        exit(1);
    }
    int len;
    if (msg == NULL)
    {
        len = strlen(str);
        if (sendall(user_info->client_fd, str, &len) == -1)
        {
            chilog(ERROR, "send error in sendall()!");
            return -1;
        }
    }
    // Todo: using msg instead of str
    // return -1 if there's a send error

    return 0;
}
