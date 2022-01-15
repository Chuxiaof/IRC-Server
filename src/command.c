#include "command.h"

#include <string.h>

#include "log.h"

typedef void (*command_func)(connect_info_handle cinfo, connect_info_handle connections, sds * tokens);
struct command_entry
{
    char *command_name;
    command_func func;
};

void process_cmd(sds command, connect_info_handle cinfo, connect_info_handle connections)
{
    chilog(INFO, "command: %s", command);

    struct command_entry command_entries[] = {
        {"NICK", send_welcome},
        {"USER", send_welcome},
    };
    int commands_num = 2; // set the number of commands manually to avoid global variable
    
    int count;
    sds *tokens = sdssplitlen(command, sdslen(command), " ", 1, &count);

    int i;
    for (i = 0; i < commands_num; i++)
    {
        if (!strcmp(tokens[0], command_entries[i].command_name)){
            command_entries[i].func(cinfo,connections, tokens);
            break;
        }
    }
    if(i==commands_num){
        chilog(WARNING, "unsupported command");
    }
    // if (strcmp(tokens[0], "NICK") == 0)
    // {
    //     cinfo->nick = sdsnew(tokens[1]);
    //     send_welcome(cinfo, connections);
    // }
    // else if (strcmp(tokens[0], "USER") == 0)
    // {
    //     cinfo->user = sdsnew(tokens[1]);
    //     send_welcome(cinfo, connections);
    // }
    // else {
    //     chilog(WARNING, "unsupported command");
    // }
}