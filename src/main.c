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

#include <sds.h>

#include "log.h"
#include "connect.h"
#include "command.h"

#define BACKLOG 5
#define MAX_BUFFER_SIZE 512

int main(int argc, char *argv[])
{
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

    struct connect_info *connections = NULL; // create the pointer to hash table

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
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size);

        connect_info_handle cinfo = (connect_info_handle)malloc(sizeof(connect_info));
        init_cinfo(cinfo);
        cinfo->host_server = host_server;
        cinfo->client_fd = client_fd;

        char host_client[1024];
        getnameinfo((struct sockaddr *)&client_addr, sizeof client_addr, host_client, sizeof host_client,
                    NULL, 0, 0);
        chilog(INFO, "host of client: %s", host_client);

        cinfo->host_client = host_client;

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

        destroy_cinfo(cinfo);
    }

    close(server_fd);

    return 0;
}
