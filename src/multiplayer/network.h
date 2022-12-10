#pragma once
#include <stdlib.h>
#include <stdbool.h>

int connect_to_server(const char *ip, int port, int retries);
bool accept_clients(int port, int num_clients, int *sockets);
bool read_from_all(int num_sock, int *sockets, void *buffers, size_t size);
int read_next(int num_sock, int *sockets, bool *exclude_mask, void *buffers, size_t size);
bool send_all(int num_sock, int *sockets, void *buffer, size_t size);
