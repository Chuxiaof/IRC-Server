/*
 *
 *  chirc: a simple multi-threaded IRC server
 *
 *  This module provides the main() function for the server,
 *  and parses the command-line arguments to the chirc executable.
 *
 */

/*
 *  Copyright (c) 2011-2020, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or withsend
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software withsend specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY send OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

#include <sds.h>
#include "uthash.h"
#include "log.h"
#include "user.h"
#include "context.h"
#include "single_service.h"
#include "message.h"
#include "command.h"

#define BACKLOG 5
#define MAX_BUFFER_SIZE 512
#define HOST_NAME_LENGTH 1024

void *service_single_client(void *args);

int main(int argc, char *argv[])
{
    sigset_t new;
    sigemptyset(&new);
    sigaddset(&new, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0)
    {
        perror("Unable to mask SIGPIPE");
        exit(-1);
    }

    // process command line arguments
    int opt;
    char *port = "6667", *passwd = NULL, *servername = NULL, *network_file = NULL;
    int verbosity = 0;

    while ((opt = getopt(argc, argv, "p:o:s:n:vqh")) != -1)
        switch (opt)
        {
        case 'p':
            port = strdup(optarg);
            break;
        case 'o':
            passwd = strdup(optarg);
            break;
        case 's':
            servername = strdup(optarg);
            break;
        case 'n':
            if (access(optarg, R_OK) == -1)
            {
                printf("ERROR: No such file: %s\n", optarg);
                exit(-1);
            }
            network_file = strdup(optarg);
            break;
        case 'v':
            verbosity++;
            break;
        case 'q':
            verbosity = -1;
            break;
        case 'h':
            printf("Usage: chirc -o OPER_PASSWD [-p PORT] [-s SERVERNAME] [-n NETWORK_FILE] [(-q|-v|-vv)]\n");
            exit(0);
            break;
        default:
            fprintf(stderr, "ERROR: Unknown option -%c\n", opt);
            exit(-1);
        }

    if (!passwd)
    {
        fprintf(stderr, "ERROR: You must specify an operator password\n");
        exit(-1);
    }

    if (network_file && !servername)
    {
        fprintf(stderr, "ERROR: If specifying a network file, you must also specify a server name.\n");
        exit(-1);
    }

    /* Set logging level based on verbosity */
    switch (verbosity)
    {
    case -1:
        chirc_setloglevel(QUIET);
        break;
    case 0:
        chirc_setloglevel(INFO);
        break;
    case 1:
        chirc_setloglevel(DEBUG);
        break;
    case 2:
        chirc_setloglevel(TRACE);
        break;
    default:
        chirc_setloglevel(TRACE);
        break;
    }

    // socket
    int server_fd, client_fd;

    pthread_t worker_thread;
    struct worker_args *wa;

    // create global context
    context_handle ctx = (context_handle)malloc(sizeof(context));
    struct user * users = NULL; // create the pointer to hash table, which stores users' info
    ctx->user_hash_table=users;

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // TODO: sockaddr_storage
    struct sockaddr_in *client_addr;
    socklen_t sin_size = sizeof(struct sockaddr_in);

    int rv, yes = 1;
    if ((rv = getaddrinfo(NULL, port, &hints, &res)) != 0)
    {
        chilog(CRITICAL, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            chilog(WARNING, "could not open socket");
            continue;
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            chilog(WARNING, "setsockopt error");
            close(server_fd);
            continue;
        }

        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            chilog(WARNING, "server: bind failed");
            close(server_fd);
            continue;
        }

        if (listen(server_fd, BACKLOG) == -1)
        {
            chilog(WARNING, "server: listen failed");
            exit(1);
        }

        break;
    }

    if (p == NULL)
    {
        chilog(CRITICAL, "could not find a socket to bind to");
        exit(1);
    }

    // get the hostname of server
    // char * server_host_name = malloc(sizeof(char)*HOST_NAME_LENGTH);
    // if((getnameinfo(p->ai_addr, p->ai_addrlen, server_host_name, HOST_NAME_LENGTH, NULL, 0, 0)!=0)){
    //     perror("getnameinfo error!");
    // }

    // get the hostname of server
    // char *server_host_name = malloc(HOST_NAME_LENGTH);
    struct hostent *he;
    // getnameinfo(p->ai_addr, p->ai_addrlen, server_host_name, HOST_NAME_LENGTH, NULL, 0, 0);
    he = gethostbyaddr(p->ai_addr, p->ai_addrlen, AF_INET);
    chilog(INFO, "server host name: %s", he->h_name);
    ctx->server_host = he->h_name;
    // chilog(INFO, "server host name: %s", server_host_name);
    // ctx->server_host=server_host_name;

    freeaddrinfo(res);

    chilog(INFO, "server: waiting for connections...");

    while (true)
    {
        if((client_addr = calloc(1, sin_size))==NULL){ // allocate client_addr for each thread
            chilog(ERROR, "fail to allocate memory for client_addr");
        } 
        if ((client_fd = accept(server_fd, (struct sockaddr *)client_addr, &sin_size)) == -1)
        {
            free(client_addr);
            chilog(INFO, "Could not accept connection");
            continue;
        }

        if((wa = calloc(1, sizeof(struct worker_args)))==NULL){// create thread function args for each thread
            chilog(ERROR, "fail to allocate memory for wa");
        } 

        user_handle user_info = create_user(); // create a user_handle for each thread
        user_info->client_fd = client_fd;
        char * client_host_name = malloc(HOST_NAME_LENGTH);
        if(client_host_name==NULL){
            chilog(ERROR, "fail to allocate memory for client_host_name");
        }
        getnameinfo((struct sockaddr *)client_addr, sin_size, client_host_name, HOST_NAME_LENGTH,
                    NULL, 0, 0);
        chilog(INFO, "client host name: %s", client_host_name);
        user_info->client_host_name = client_host_name;

        // construct arguments for thread function
        wa->user_info = user_info;
        wa->ctx = ctx;

        // free(client_addr);

        if (pthread_create(&worker_thread, NULL, service_single_client, wa) != 0)
        {
            perror("could not create a worker thread");
            free(wa);
            close(client_fd);
            close(server_fd);
            return EXIT_FAILURE;
        }
    }

    close(server_fd);

    return 0;
}

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
