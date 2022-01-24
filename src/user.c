#include "user.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sds.h>
#include <stdbool.h>

#include "reply.h"
#include "log.h"

user_handle create_user() {
    user_handle user = (user_handle) malloc(sizeof(user_t));
    if (user == NULL) {       
        chilog(ERROR, "fail to create user: no enough memory");
        return NULL;
    }
    user->nick = NULL;
    user->username = NULL;
    user->username = NULL;
    user->registered = false;
    return user;
}

void destroy_user(user_handle user) {
    if (user != NULL) {
        // TODO
        // depend on the implementation of char *
        // string.h or sds
    }
    free(user);
}

bool can_register(user_handle user) {
    if (user == NULL) {
        chilog(CRITICAL, "can_register: empty user_handle");
        exit(1);
    }
    return user->nick != NULL && user->username != NULL;
}

void send_welcome(user_handle user, char *server_host_name) {
    if (user == NULL) {
        chilog(CRITICAL, "send_welcome: empty user_handle");
        exit(1);
    }

    if (server_host_name == NULL) {
        chilog(WARNING, "send_welcome: server_host_name null");
    }

    char msg[512];
    sprintf(msg, ":%s %s %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
            server_host_name, RPL_WELCOME, user->nick,
            user->nick, user->username, user->client_host_name);
    send(user->client_fd, msg, strlen(msg), 0);
    chilog(INFO, "send welcome message to %s@%s", user->nick, user->client_host_name);
}
