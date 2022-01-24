#ifndef HANDLERS_H
#define HANDLERS_H

#include "context.h"
#include "user.h"
#include "message.h"

int handler_NICK(context_handle ctx, user_handle user_info, message_handle msg);

int handler_USER(context_handle ctx, user_handle user_info, message_handle msg);

int handler_QUIT(context_handle ctx, user_handle user_info, message_handle msg);

int handler_LUSERS(context_handle ctx, user_handle user_info, message_handle msg);

#endif