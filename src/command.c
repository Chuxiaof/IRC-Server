#include "command.h"

#include <string.h>

#include "log.h"

void process_cmd(sds command, connect_info_handle cinfo) {
    chilog(INFO, "command: %s", command);

    int count;
    sds *tokens = sdssplitlen(command, sdslen(command), " ", 1, &count);

    if (strcmp(tokens[0], "NICK") == 0)
    {
        cinfo->nick = sdsnew(tokens[1]);
        send_welcome(cinfo);
    }
    else if (strcmp(tokens[0], "USER") == 0)
    {
        cinfo->user = sdsnew(tokens[1]);
        send_welcome(cinfo);
    }
    else {
        chilog(WARNING, "unsupported command");
    }
}