#include "handler.h"

#include <sds.h>
#include <stdbool.h>

#include "user.h"
#include "context.h"
#include "message.h"
#include "log.h"

bool check_param_number(int have, int target);

void handler_NICK(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (!check_param_number(msg->nparams, 1)) {
        chilog(ERROR, "handler_NICK: insufficient params");
        // TODO: error handling
        return;
    }

    if (user_info->registered) {
        // change nick
    } else {
        user_info->nick = msg->params[0];
        if (can_register(user_info)) {
            // add to ctx->user_table
            user_info->registered = true;
            // send welcome
            // TODO, register
            send_welcome(user_info, ctx->server_host);
        }
    }

    //     HASH_ADD_KEYPTR(hh, connections, cinfo->nick, strlen(cinfo->nick), cinfo);
}

void handler_USER(context_handle ctx, user_handle user_info, message_handle msg)
{
    if (!check_param_number(msg->nparams, 4)) {
        chilog(ERROR, "handler_USER: insufficient params");
        return;
        //TODO
    }

    if (user_info->registered) {
        // error handling
    } else {
        user_info->username = msg->params[0];
        user_info->fullname = msg->params[3];
        if (can_register(user_info)) {
            // add to ctx->user_table
            user_info->registered = true;
            // send welcome
            // TODO, register
            send_welcome(user_info, ctx->server_host);
        }
    }

    // HASH_ADD_KEYPTR(hh, connections, cinfo->nick, strlen(cinfo->nick), cinfo);
}

bool check_param_number(int have, int target) {
    return have >= target;
}