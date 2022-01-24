#include "single_service.h"

#include <sds.h>

#include "log.h"
#include "message.h"

#define MAX_BUFFER_SIZE 512

void *service_single_client(void *args)
{
    // extract arguments
    struct worker_args *wa = (struct worker_args *)args;
    context_handle ctx = wa->ctx;
    user_handle user_info = wa->user_info;

    pthread_detach(pthread_self());

    char recv_msg[MAX_BUFFER_SIZE];
    char buffer[MAX_BUFFER_SIZE];
    int ptr = 0;
    bool flag = false;

    while (true)
    {
        int len = recv(user_info->client_fd, recv_msg, MAX_BUFFER_SIZE, 0);

        if (len == 0)
        {
            chilog(INFO, "client %s disconnected", user_info->client_host_name);
            close(user_info->client_fd);
            pthread_exit(NULL);
        }

        if (len == -1)
        {
            chilog(ERROR, "recv from %s fail", user_info->client_host_name);
            close(user_info->client_fd);
            pthread_exit(NULL);
        }

        chilog(DEBUG, "recv_msg: %s", recv_msg);

        for (int i = 0; i < len; i++)
        {
            char c = recv_msg[i];
            if (c == '\n' && flag)
            {
                // whenever we identify a complete command
                sds command = sdsempty();
                command = sdscpylen(command, buffer, ptr - 1);
                // create a message
                message_handle msg = malloc(sizeof(message_t));
                // transform command into message
                message_from_string(msg, command);
                // process the message
                process_cmd(ctx, user_info, msg);
                // TODO: free message
                // after processing a command, continue to analyze the next command
                flag = false;
                ptr = 0;
                continue;
            }
            buffer[ptr++] = c;
            flag = c == '\r';
        }
    }
}