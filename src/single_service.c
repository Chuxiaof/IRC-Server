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

    while (true)
    {
        int len = recv(user_info->client_fd, recv_msg, MAX_BUFFER_SIZE, 0);

        if (len == 0)
        {
            chilog(INFO, "client %s disconnected", user_info->client_host_name);
            close(user_info->client_fd);
            pthread_mutex_lock(&ctx->lock_connection_table);
            delete_connection(&(ctx->connection_hash_table), user_info->client_fd);
            pthread_mutex_unlock(&ctx->lock_connection_table);
            pthread_mutex_lock(&ctx->lock_user_table);
            delete_user(&(ctx->user_hash_table), user_info);
            pthread_mutex_unlock(&ctx->lock_user_table);
            pthread_exit(NULL);
        }

        if (len == -1)
        {
            chilog(ERROR, "recv from %s fail", user_info->client_host_name);
            close(user_info->client_fd);
            pthread_mutex_lock(&ctx->lock_connection_table);
            delete_connection(&(ctx->connection_hash_table), user_info->client_fd);
            pthread_mutex_unlock(&ctx->lock_connection_table);
            pthread_mutex_lock(&ctx->lock_user_table);
            delete_user(&(ctx->user_hash_table), user_info);
            pthread_mutex_unlock(&ctx->lock_user_table);
            pthread_exit(NULL);
        }

        chilog(DEBUG, "recv_msg: %s", recv_msg);

        for (int i = 0; i < len; i++)
        {
            char c = recv_msg[i];
            if (c == '\n' && flag)
            {
                // whenever we identify a complete command
                sds command = sdscpylen(sdsempty(), buffer, ptr - 1);
                // create a message
                message_handle msg = malloc(sizeof(message_t));
                // transform command into message
                message_from_string(msg, command);
                // process the message
                if (process_cmd(ctx, user_info, msg) == -1)
                {
                    // if there's an error during processing this command, then kill this thread
                    close(user_info->client_fd);
                    free(wa->ctx);
                    free(wa->user_info);
                    free(wa);

                    // Delete this user from hash table
                    //  if(user_info->nick){
                    //      user_handle temp;
                    //      pthread_mutex_lock(&ctx->lock_user_table);
                    //      HASH_FIND_STR(ctx->user_hash_table, user_info->nick, temp);
                    //      if(temp){
                    //          HASH_DEL(ctx->user_hash_table, temp);
                    //      }
                    //      pthread_mutex_unlock(&ctx->lock_user_table);
                    //  }
                    pthread_mutex_lock(&ctx->lock_user_table);
                    delete_user(&(ctx->user_hash_table), user_info);
                    pthread_mutex_unlock(&ctx->lock_user_table);

                    pthread_mutex_lock(&ctx->lock_connection_table);
                    delete_connection(&(ctx->connection_hash_table), user_info->client_fd);
                    pthread_mutex_unlock(&ctx->lock_connection_table);
                    pthread_exit(NULL);
                }
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