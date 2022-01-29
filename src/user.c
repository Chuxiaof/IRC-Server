#include "user.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sds.h>
#include <stdbool.h>
#include "reply.h"
#include "log.h"

user_handle create_user()
{
    user_handle user = calloc(1, sizeof(user_t));
    if (user == NULL) {
        chilog(ERROR, "fail to create user: no enough memory");
        return NULL;
    }
    user->nick = NULL;
    user->username = NULL;
    user->username = NULL;
    user->registered = false;
    user->is_irc_operator = false;
    return user;
}

void destroy_user(user_handle user)
{
    if (user != NULL) {
        sdsfree(user->nick);
        sdsfree(user->username);
        sdsfree(user->fullname);
        free(user->client_host_name);
    }
    free(user);
}

bool can_register(user_handle user)
{
    return user->nick != NULL && user->username != NULL;
}
