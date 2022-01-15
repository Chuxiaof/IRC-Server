#include "command.h"

#include <string.h>

#include "log.h"
#include "handler.h"

typedef void (*command_func)(connect_info_handle cinfo, connect_info_handle connections, sds command);

struct command_entry
{
    char *command_name;
    command_func func;
};

struct command_entry command_entries[] = {
    {"NICK", handler_NICK},
    {"USER", handler_USER},
};

int commands_num = sizeof(command_entries) / sizeof(struct command_entry);

void process_cmd(sds command, connect_info_handle cinfo, connect_info_handle connections)
{
    chilog(INFO, "command: %s", command);

    int count;
    sds *tokens = sdssplitlen(command, sdslen(command), " ", 1, &count);

    int i;
    for (i = 0; i < commands_num; i++)
    {
        if (!strcmp(tokens[0], command_entries[i].command_name))
        {
            command_entries[i].func(cinfo, connections, command);
            break;
        }
    }
    if (i == commands_num)
    {
        chilog(WARNING, "unsupported command");
    }
}