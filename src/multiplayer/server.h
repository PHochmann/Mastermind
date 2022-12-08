#pragma once
#include "server.h"
#include "protocol.h"
#include "../mastermind.h"

void start_server(MM_Context *context, int num_players, int num_rounds, int port);
