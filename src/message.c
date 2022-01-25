#include "message.h"

#include <stdlib.h>
#include <sds.h>
#include <stdio.h>

#include "log.h"

bool empty_string(char *str);

int message_from_string(message_handle msg, char *s)
{
    if (msg == NULL)
    {
        chilog(ERROR, "message_from_string: empty message_handle");
        return -1;
    }
    if (empty_string(s))
    {
        chilog(ERROR, "message_from_string: empty string");
        return -1;
    }

    // first split the string by :
    int count_1 = 0;
    sds *split = sdssplitlen(s, sdslen(s), ":", 1, &count_1);

    // then split the first part by " "
    int count_2 = 0;
    sds before_colon = split[0];
    sds *tokens = sdssplitlen(before_colon, sdslen(before_colon), " ", 1, &count_2);

    msg->cmd = tokens[0];
    msg->nparams = count_2 - 1;
    for (int i = 0; i < msg->nparams; i++)
        if (tokens[i + 1] != NULL && sdslen(tokens[i + 1]) > 0)
            msg->params[i] = tokens[i + 1];

    if (count_1 > 1)
    {
        sdsrange(s, sdslen(before_colon) + 1, sdslen(s) - 1);
        msg->params[msg->nparams++] = s;
        msg->longlast = true;
    }
    else
    {
        msg->longlast = false;
    }

    return 0;
}

int message_to_string(message_handle msg, char *s)
{
    // :preix cmd [params] :longlast
    // prefix: required
    // cmd: not required
    sds params;
    if (msg->longlast)
    {
        params = sdsjoin(msg->params, msg->nparams - 1, " ");
        params = sdscatfmt(params, "%s :%s", params, msg->params[msg->nparams - 1]);
    }
    else
    {
        params = sdsjoin(msg->params, msg->nparams, " ");
    }

    if (msg->cmd != NULL)
    {
        sprintf(s, ":%s %s %s",
                msg->prefix, msg->cmd, params);
    }
    else
    {
        sprintf(s, ":%s %s", msg->prefix, params);
    }

    return 0;
}

int message_construct(message_handle msg, char *prefix, char *cmd)
{
    if (msg == NULL)
    {
        chilog(ERROR, "message_construct: empty message_handle");
        return -1;
    }

    if (empty_string(prefix))
    {
        chilog(WARNING, "message_construct: empty prefix");
    }

    if (empty_string(cmd))
    {
        chilog(WARNING, "message_construct: empty cmd");
    }

    msg->prefix = prefix;
    msg->cmd = cmd;
    return 0;
}

int message_add_parameter(message_handle msg, char *param, bool longlast)
{
    if (msg == NULL)
    {
        chilog(ERROR, "message_add_parameter: empty message_handle");
        return -1;
    }

    if (msg->nparams == 15)
    {
        chilog(ERROR, "message_add_parameter: already 15 params, can't add more");
        return -1;
    }

    if (longlast)
    {
        chilog(ERROR, "message_add_parameter: longlast is true, can't add params any more");
        return -1;
    }

    if (empty_string(param))
    {
        chilog(WARNING, "message_add_parameter: do not add empty param (ignored)");
        return 0;
    }

    msg->params[msg->nparams++] = param;
    msg->longlast = longlast;
    return 0;
}

int message_destroy(message_handle msg)
{
    return 0;
}

bool empty_string(char *str)
{
    return str == NULL || sdslen(str) == 0;
}