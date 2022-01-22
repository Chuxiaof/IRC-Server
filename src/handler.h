#ifndef HANDLERS_H
#define HANDLERS_H

#include "context.h"
#include "user.h"
#include "message.h"

void handler_NICK(context_handle ctx, user_handle user_info, message_handle msg);

void handler_USER(context_handle ctx, user_handle user_info, message_handle msg);

#endif