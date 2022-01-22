#include "command.h"

#include <string.h>

#include "log.h"
#include "handler.h"

typedef void (*handler_func)(context_handle ctx, user_handle user_info, message_handle msg);

struct handler_entry
{
    char * command_name;
    handler_func func;
};

static struct handler_entry handler_entries[] = {
    {"NICK", handler_NICK},
    {"USER", handler_USER},
};

int handlers_num = sizeof(handler_entries) / sizeof(struct handler_entry);

void process_cmd(context_handle ctx, user_handle user_info, message_handle msg)
{
    int i;
    for (i = 0; i < handlers_num; i++)
    {
        if (!strcmp(msg->cmd, handler_entries[i].command_name))
        {
            handler_entries[i].func(ctx, user_info, msg);
            break;
        }
    }
    if (i == handlers_num)
    {
        // TODO 
        chilog(WARNING, "unsupported command");
    }
}