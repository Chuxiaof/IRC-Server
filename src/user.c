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




