#include "connect.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>

#include "reply.h"
#include "log.h"

void send_welcome(connect_info_handle cinfo, connect_info_handle connections, sds * tokens)
{   
    if(!strcmp(tokens[0],"NICK")){
        cinfo->nick = sdsnew(tokens[1]);
    }else{
        cinfo->user = sdsnew(tokens[1]);
    }
    if (cinfo->nick != NULL && cinfo->user != NULL) {
        char msg[1024];
        sprintf(msg, ":%s %s %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
                cinfo->host_server, RPL_WELCOME, cinfo->nick,
                cinfo->nick, cinfo->user, cinfo->host_client);
        send(cinfo->client_fd, msg, strlen(msg), 0);
        chilog(INFO, "send welcome message to %s@%s", cinfo->nick, cinfo->host_client);
        cinfo->registered = true;
        HASH_ADD_KEYPTR(hh,connections,cinfo->nick,strlen(cinfo->nick),cinfo);
    }
}

void destroy_cinfo(connect_info_handle cinfo) {
    free(cinfo);
}