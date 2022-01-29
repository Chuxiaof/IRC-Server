#ifndef COMMAND_H
#define COMMAND_H

#include "context.h"
#include "user.h"
#include "message.h"

/* According to the command(NICK, USER, JOIN...), call the corresponding function using dispatch table */
int process_cmd(context_handle ctx, user_handle user_info, message_handle msg);

#endif