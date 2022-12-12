#pragma once
#include "../mastermind.h"
#include "protocol.h"
#include "server.h"

void start_server(MM_Context *context, int num_players, int num_rounds, int port);
