#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>

#include "network.h"
#include "string_util.h"
#include "protocol.h"
#include "console.h"

int connect_to_server(const char *ip, int port)
{
    int socket_to_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_to_server < 0)
    {
        printf("Error creating socket.\n");
        return -1;
    }

    struct in_addr serv_addr;
    struct sockaddr_in server_ip;
    serv_addr.s_addr = inet_addr(ip);
    server_ip.sin_family = AF_INET;
    server_ip.sin_port = htons(port);
    server_ip.sin_addr = serv_addr;

    if (connect(socket_to_server, (struct sockaddr*)&server_ip, sizeof(server_ip)) == -1)
    {
        printf("Error connecting to server\n");
        close(socket_to_server);
        return -1;
    }

    return socket_to_server;
}

bool accept_clients(int port, int num_clients, int *sockets)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
        printf("Error creating listening socket.\n");
        return false;
    }

    struct in_addr ip; // Create IP
    ip.s_addr = htonl(INADDR_ANY); // Use any IP
    struct sockaddr_in sockaddr; // Struct for socket address for TCP/IP
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr = ip;

    // Binds address to server's socket
    // Client calls connect() on the same address
    // Signature of bind() and connect() is the same
    if (bind(listen_sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1)
    {
        printf("Failed binding address to socket\n");
        close(listen_sock);
        return false;
    }

    if (listen(listen_sock, num_clients) < 0)
    {
        printf("Listen failed\n");
        close(listen_sock);
        return false;
    }

    for (uint8_t i = 0; i < num_clients; i++)
    {
        printf("Waiting for player %d/%d...", i + 1, num_clients);
        fflush(stdout);
        struct sockaddr_in client_addr;
        unsigned int addr_size;
        sockets[i] = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_size);
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
    int count = 0;
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

int read_next(int num_sock, int *sockets, bool *exclude_mask, void *buffers, size_t size)
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
            if (recv(sockets[i], (uint8_t*)buffers + i * size, size, 0) < 0)
            {
                return -1;
            }
            return i;
        }
    }

    return -1;
}
