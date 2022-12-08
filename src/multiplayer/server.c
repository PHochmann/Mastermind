#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#include "server.h"
#include "protocol.h"
#include "network.h"
#include "../mastermind.h"

void start_server(MM_Context *ctx, int num_players, int num_rounds, int port)
{
    printf("Starting server...\n");
    int sockets[MAX_NUM_PLAYERS];
    if (!accept_clients(port, num_players, sockets))
    {
        return;
    }
    printf("[1] All players connected. Waiting \n");

    bool nickname_validated[MAX_NUM_PLAYERS];
    NicknamePackage_Q nicknames[MAX_NUM_PLAYERS];
    int nickname_validated_count = 0;

    while (nickname_validated_count != num_players)
    {
        int player = read_next(num_players, sockets, nickname_validated, nicknames, sizeof(NicknamePackage_Q));
        bool duplicate = false;
        for (int i = 0; i < num_players; i++)
        {
            if (nickname_validated[i])
            {
                if (strcmp(nicknames[player].name, nicknames[i].name) == 0)
                {
                    duplicate = true;
                    break;
                }
            }
        }
        NicknamePackage_R response;
        nickname_validated[player] = !duplicate;
        response.is_accepted = !duplicate;
        send(sockets[player], &response, sizeof(NicknamePackage_R), 0);
        printf("Got nickname from player %d: %s, ", player, nicknames[player].name);
        if (duplicate)
        {
            printf("duplicate\n");
        }
        else
        {
            printf("accepted\n");
            nickname_validated_count++;
        }
    }

    printf("[2] All players have nicknames. Sending rules.\n");
    GameBeginPackage_R begin_package = (GameBeginPackage_R){
        .max_guesses = mm_get_max_guesses(ctx),
        .num_colors = mm_get_num_colors(ctx),
        .num_players = num_players,
        .num_slots = mm_get_num_slots(ctx),
    };

    send_all(num_players, sockets, &begin_package, sizeof(GameBeginPackage_R));

    printf("Game ended, stopping server.\n");
    for (uint8_t i = 0; i < num_players; i++)
    {
        close(sockets[i]);
    }
}
