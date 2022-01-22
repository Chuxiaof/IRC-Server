#ifndef COMMAND_H
#define COMMAND_H

#include "context.h"
#include "user.h"
#include "message.h"

void process_cmd(context_handle ctx, user_handle user_info, message_handle msg);

#endif