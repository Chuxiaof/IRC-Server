#ifndef HANDLERS_H
#define HANDLERS_H

#include "context.h"
#include "user.h"
#include "message.h"

/*
Below are all handler functions corresponding to different client-side commands
*/

int handler_NICK(context_handle ctx, user_handle user_info, message_handle msg);

int handler_USER(context_handle ctx, user_handle user_info, message_handle msg);

int handler_PRIVMSG(context_handle ctx, user_handle user_info, message_handle msg);

int handler_NOTICE(context_handle ctx, user_handle user_info, message_handle msg);

int handler_PING(context_handle ctx, user_handle user_info, message_handle msg);

int handler_PONG(context_handle ctx, user_handle user_info, message_handle msg);

int handler_WHOIS(context_handle ctx, user_handle user_info, message_handle msg);

int handler_UNKNOWNCOMMAND(context_handle ctx, user_handle user_info, message_handle msg);

int handler_QUIT(context_handle ctx, user_handle user_info, message_handle msg);

int handler_LUSERS(context_handle ctx, user_handle user_info, message_handle msg);

int handler_JOIN(context_handle ctx, user_handle user_info, message_handle msg);

int handler_PART(context_handle ctx, user_handle user_info, message_handle msg);

int handler_LIST(context_handle ctx, user_handle user_info, message_handle msg);

int handler_OPER(context_handle ctx, user_handle user_info, message_handle msg);

int handler_MODE(context_handle ctx, user_handle user_info, message_handle msg);

#endif