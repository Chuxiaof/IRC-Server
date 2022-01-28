#include "single_service.h"

#include <sds.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "message.h"
#include "command.h"

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

    while (true) {
        int len = recv(user_info->client_fd, recv_msg, MAX_BUFFER_SIZE, 0);

        if (len == 0) {
            chilog(INFO, "client %s disconnected", user_info->client_host_name);
            close(user_info->client_fd);
            free_data(wa);
            pthread_exit(NULL);
        }

        if (len == -1) {
            chilog(ERROR, "recv from %s fail", user_info->client_host_name);
            close(user_info->client_fd);
            free_data(wa);
            pthread_exit(NULL);
        }

        chilog(DEBUG, "recv_msg: %s", recv_msg);

        for (int i = 0; i < len; i++) {
            char c = recv_msg[i];
            if (c == '\n' && flag) {
                sds command = sdscpylen(sdsempty(), buffer, ptr - 1);
                message_handle msg = calloc(1, sizeof(message_t));
                if (msg == NULL) {
                    chilog(CRITICAL, "single_service: fail to allocate new memory");
                    exit(1);
                }
                message_from_string(msg, command);
                if (process_cmd(ctx, user_info, msg) == -1) {
                    // if there's an error during processing this command, then kill this thread
                    close(user_info->client_fd);
                    free_data(wa);
                    pthread_exit(NULL);
                }
                // free message
                message_destroy(msg);
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

void free_data(worker_args *args) {
    delete_connection(args->ctx, args->user_info->client_fd);
    delete_user(args->ctx, args->user_info);
    destroy_user(args->user_info);
    free(args);
}