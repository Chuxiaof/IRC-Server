#include "command.h"

#include <string.h>

#include "log.h"
#include "handler.h"

typedef int (*handler_func)(context_handle ctx, user_handle user_info, message_handle msg);

struct handler_entry
{
    char *command_name;
    handler_func func;
};

static struct handler_entry handler_entries[] = {
    {"NICK", handler_NICK},
    {"USER", handler_USER},
    {"PRIVMSG", handler_PRIVMSG},
    {"NOTICE", handler_NOTICE},
    {"PING", handler_PING},
    {"PONG", handler_PONG},
    {"WHOIS", handler_WHOIS},
    {"QUIT", handler_QUIT},
    {"LUSERS", handler_LUSERS},
    {"JOIN", handler_JOIN}
};

int handlers_num = sizeof(handler_entries) / sizeof(struct handler_entry);

int process_cmd(context_handle ctx, user_handle user_info, message_handle msg)
{
    int i;
    for (i = 0; i < handlers_num; i++)
    {
        if (!strcmp(msg->cmd, handler_entries[i].command_name))
        {
            return handler_entries[i].func(ctx, user_info, msg);
        }
    }
    if (i == handlers_num)
    {
        chilog(WARNING, "unsupported command");
        return handler_UNKNOWNCOMMAND(ctx, user_info, msg);
    }
    return 0;
}