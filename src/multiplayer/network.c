#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <readline/readline.h>

#include "console.h"
#include "network.h"
#include "protocol.h"
#include "string_util.h"

#define RETRY_WAIT_MS 100

int connect_to_server(const char *ip, int port, int retries)
{
    int socket_to_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_to_server < 0)
    {
        printf("Error creating socket.\n");
        return -1;
    }

    struct in_addr serv_addr;
    struct sockaddr_in server_ip;
    serv_addr.s_addr     = inet_addr(ip);
    server_ip.sin_family = AF_INET;
    server_ip.sin_port   = htons(port);
    server_ip.sin_addr   = serv_addr;

    while (retries != 0)
    {
        if (connect(socket_to_server, (struct sockaddr *)&server_ip, sizeof(server_ip)) == -1)
        {
            if (retries > 0)
            {
                retries--;
            }
            usleep(RETRY_WAIT_MS * 1000);
        }
        else
        {
            return socket_to_server;
        }
    }

    printf("Error connecting to server.\n");
    return -1;
}

bool accept_clients(int port, int num_clients, int *sockets)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
        printf("Error creating listening socket.\n");
        return false;
    }

    struct in_addr ip;             // Create IP
    ip.s_addr = htonl(INADDR_ANY); // Use any IP
    struct sockaddr_in sockaddr;   // Struct for socket address for TCP/IP
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port   = htons(port);
    sockaddr.sin_addr   = ip;

    // Binds address to server's socket
    // Client calls connect() on the same address
    // Signature of bind() and connect() is the same
    int bind_code = bind(listen_sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (bind_code < 0)
    {
        printf("Failed binding address to socket: %d\n", bind_code);
        close(listen_sock);
        return false;
    }

    if (listen(listen_sock, num_clients) < 0)
    {
        printf("Listen failed\n");
        close(listen_sock);
        return false;
    }
    else
    {
        printf("Listening on port %d\n", port);
    }

    for (int i = 0; i < num_clients; i++)
    {
        printf("Waiting for player %d/%d...", i + 1, num_clients);
        fflush(stdout);
        struct sockaddr_in client_addr;
        unsigned int addr_size;
        sockets[i] = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (sockets[i] < 0)
        {
            printf("\nError accepting new client, code: %d\n", sockets[i]);
            close(listen_sock);
            return false;
        }
        char clntName[INET_ADDRSTRLEN]; // String to contain client address
        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, clntName, sizeof(clntName));
        printf("connected to %s\n", clntName);
    }

    close(listen_sock);
    return true;
}

bool read_from_all(int num_sock, int *sockets, void *buffers, size_t size)
{
    bool mask[MAX_NUM_PLAYERS] = { 0 };
    int count                  = 0;
    while (count < num_sock)
    {
        int res = read_next(num_sock, sockets, mask, buffers, size);
        if (res < 0)
        {
            return false;
        }
        mask[res] = true;
        count++;
    }
    return true;
}

bool send_all(int num_sock, int *sockets, void *buffer, size_t size)
{
    for (int i = 0; i < num_sock; i++)
    {
        if (send(sockets[i], buffer, size, 0) < 0)
        {
            return false;
        }
    }
    return true;
}

int read_next(int num_sock, int *sockets, bool *exclude_mask, void *buffer, size_t size)
{
    fd_set clients;
    FD_ZERO(&clients);
    for (int i = 0; i < num_sock; i++)
    {
        if (!exclude_mask[i])
        {
            FD_SET(sockets[i], &clients);
        }
    }

    if (select(FD_SETSIZE, &clients, NULL, NULL, NULL) < 0)
    {
        return -1;
    }

    for (int i = 0; i < num_sock; i++)
    {
        if (!exclude_mask[i] && FD_ISSET(sockets[i], &clients))
        {
            if (recv(sockets[i], (uint8_t *)buffer, size, 0) < 0)
            {
                return -1;
            }
            return i;
        }
    }

    return -1;
}
