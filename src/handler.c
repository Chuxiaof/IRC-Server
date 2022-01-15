#include "handler.h"

#include <sds.h>

#include "connect.h"

void handler_NICK(connect_info_handle cinfo, connect_info_handle connections, sds command)
{
    // TODO
    // check whether current connection is already registered

    int count;
    sds *tokens = sdssplitlen(command, sdslen(command), " ", 1, &count);

    cinfo->nick = sdsnew(tokens[1]);
    if (cinfo->nick != NULL && cinfo->user != NULL)
    {
        send_welcome(cinfo);
        HASH_ADD_KEYPTR(hh, connections, cinfo->nick, strlen(cinfo->nick), cinfo);
    }
}

void handler_USER(connect_info_handle cinfo, connect_info_handle connections, sds command)
{
    // TODO
    // same above

    int count;
    sds *tokens = sdssplitlen(command, sdslen(command), " ", 1, &count);

    cinfo->user = sdsnew(tokens[1]);
    if (cinfo->nick != NULL && cinfo->user != NULL)
    {
        send_welcome(cinfo);
        HASH_ADD_KEYPTR(hh, connections, cinfo->nick, strlen(cinfo->nick), cinfo);
    }
}