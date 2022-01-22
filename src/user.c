#include "user.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sds.h>

#include "context.h"
#include "reply.h"
#include "log.h"

void register(context_handle ctx, user_handle user);

user_handle create_user() {
    user_handle user = (user_handle) malloc(sizeof(user));
    if (user == null) {
        chilog(ERROR, "fail to create user: no enough memory");
        return NULL;
    }
    user->nick = NULL;
    user->username = NULL;
    user->username = NULL;
    user->registered = false;
    return user;
}

void delete_user(context_handle ctx, user_handle user) {
    if (user != NULL) {
        // TODO
        // depend on the implementation of char *
        // string.h or sds
    }
    free(user);
}

void update_nick(context_handle ctx, user_handle user, char *nick) {
    if (user == NULL) {
        chilog(ERROR, "update_nick: user is null");
        return;
    }
    if (nick == NULL || sdslen(nick) == 0) {
        chilog(ERROR, "update_nick: nick is empty");
        return;
    }
    user->nick = nick;

}

void update_username(context_handle ctx, user_handle user, char *username, char *fullname);

void send_welcome(context_handle ctx, user_handle user) {
    char msg[512];
    sprintf(msg, ":%s %s %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
            ctx->server_host, RPL_WELCOME, user->nick,
            user->nick, user->username, user->client_host);
    send(user->client_fd, msg, strlen(msg), 0);
    chilog(INFO, "send welcome message to %s@%s", user->nick, user->client_host);
    user->registered = true;
}

