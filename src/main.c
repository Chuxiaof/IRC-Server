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

#define BACKLOG 5
#define MAX_BUFFER_SIZE 512

void * service_single_client(void * args);

int main(int argc, char *argv[])
{
    /* If a client closes a connection, this will generally produce a SIGPIPE
       signal that will kill the process. We want to ignore this signal, so
       send() just returns -1 when this happens. */
    sigset_t new;
    sigemptyset (&new);
    sigaddset(&new, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0) 
    {
        perror("Unable to mask SIGPIPE");
        exit(-1);
    }

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

    /* Your code goes here */

    int server_fd, client_fd;

    pthread_t worker_thread;
    struct work_args * wa;

    struct context * ctx = malloc(sizeof(struct context)); //create global context
    struct user * users = NULL; // create the pointer to hash table, which stores users' info
    ctx->users_hash_table=users;

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct sockaddr_in client_addr;
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
            chilog(WARNING, "server: socket error");
            continue;
        }
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            chilog(CRITICAL, "setsockopt error");
            exit(1);
        }
        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(server_fd);
            chilog(WARNING, "server: bind error");
            continue;
        }
        break;
    }

    freeaddrinfo(res);
    if (p == NULL)
    {
        chilog(CRITICAL, "server: failed to bind");
        exit(1);
    }
    if (listen(server_fd, BACKLOG) == -1)
    {
        chilog(CRITICAL, "listen error");
        exit(1);
    }

    char host_server[1024];
    gethostname(host_server, sizeof host_server);
    chilog(INFO, "host of server: %s", host_server);

    chilog(INFO, "server: waiting for connections...");

    while (true)
    {   
        
        if((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size))==-1){
            chilog(INFO, "Could not accept() connection");
            continue;
        }
        wa = malloc(sizeof(struct work_args));
        

        //construct user_info for this connection
        user_handle user_info = create_user();
        user_info->client_fd = client_fd;
        char client_host_name[1024];
        getnameinfo((struct sockaddr *)&client_addr, sizeof client_addr, client_host_name, sizeof client_host_name,
                    NULL, 0, 0);
        chilog(INFO, "client host name: %s", client_host_name);
        user->client_host = client_host_name;
        
        //construct arguments for thread function
        wa->user_info=user_info;
        wa->ctx=ctx;

        if (pthread_create(&worker_thread, NULL, service_single_client, wa) != 0)
        {
            perror("Could not create a worker thread");
            free(wa);
            close(client_fd);
            close(server_fd);
            return EXIT_FAILURE;
        }
    }

    close(server_fd);

    return 0;
}

void * service_single_client(void * args){
        //extract arguments
        struct work_args * wa=(struct work_args *)args;
        context_handle ctx=wa->ctx;
        user_handle user_info=wa->user_info;

        
        char recv_msg[MAX_BUFFER_SIZE];
        char buffer[MAX_BUFFER_SIZE];
        int ptr = 0;
        bool flag = false;

        while (true)
        {
            int len = recv(client_fd, &recv_msg, MAX_BUFFER_SIZE, 0);
            chilog(DEBUG, "recv_msg: %s", recv_msg);
            if (len == -1)
            {
                chilog(ERROR, "recv from %d fail", client_fd);
                continue;
            }
            for (int i = 0; i < len; i++)
            {
                char c = recv_msg[i];
                if (c == '\n' && flag)
                {
                    sds command = sdsempty();
                    command = sdscpylen(command, buffer, ptr - 1);
                    process_cmd(command, cinfo, connections);
                    // connections: pointer to hash table
                    flag = false;
                    ptr = 0;
                    continue;
                }
                buffer[ptr++] = c;
                flag = c == '\r';
            }
        }
}
