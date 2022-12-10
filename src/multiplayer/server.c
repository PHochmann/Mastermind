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
#include "../util/console.h"

void start_server(MM_Context *ctx, int num_players, int num_rounds, int port)
{
    printf("Starting server...\n");
    int sockets[MAX_NUM_PLAYERS];
    if (!accept_clients(port, num_players, sockets))
    {
        return;
    }
    printf("All players connected. Validating nicknames... \n");

    bool nickname_validated[MAX_NUM_PLAYERS];
    char nicknames[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_LENGTH];
    int nickname_validated_count = 0;

    while (nickname_validated_count != num_players)
    {
        NicknamePackage_Q next_nickname;
        int player = read_next(num_players, sockets, nickname_validated, &next_nickname, sizeof(NicknamePackage_Q));
        bool duplicate = false;
        for (int i = 0; i < num_players; i++)
        {
            if (nickname_validated[i])
            {
                if (strcmp(next_nickname.name, nicknames[i]) == 0)
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
        printf("Got nickname from player[%d]: %s, ", player, next_nickname.name);
        if (duplicate)
        {
            printf("duplicate\n");
        }
        else
        {
            printf("accepted\n");
            strncpy(nicknames[player], next_nickname.name, MAX_PLAYER_NAME_LENGTH);
            nickname_validated_count++;
        }
    }

    printf("[2] All players have nicknames. Sending rules.\n");
    GameBeginPackage_R begin_package = (GameBeginPackage_R){
        .num_players = num_players,
        .num_rounds = num_rounds,
        .max_guesses = mm_get_max_guesses(ctx),
        .num_colors = mm_get_num_colors(ctx),
        .num_slots = mm_get_num_slots(ctx),
    };
    for (int i = 0; i < num_players; i++)
    {
        strncpy(begin_package.players[i], nicknames[i], MAX_PLAYER_NAME_LENGTH);
    }
    send_all(num_players, sockets, &begin_package, sizeof(GameBeginPackage_R));

    for (int i = 0; i < num_rounds; i++)
    {
        RoundBeginPackage_R new_round_package;
        send_all(num_players, sockets, &new_round_package, sizeof(RoundBeginPackage_R));

        uint16_t solution = rand() % mm_get_num_codes(ctx);
        printf("Round %d/%d\n", i + 1, num_rounds);
        printf("Solution: ");
        print_colors(ctx, solution);
        printf("\n");

        MM_Match *matches[MAX_NUM_PLAYERS];
        for (int j = 0; j < num_players; j++)
        {
            matches[j] = mm_new_match(ctx, false);
        }

        bool players_finished[MAX_NUM_PLAYERS] = { false };
        int num_players_finished = 0;

        while (num_players_finished != num_players)
        {
            GuessPackage_Q guess;
            int player = read_next(num_players, sockets, players_finished, &guess, sizeof(GuessPackage_Q));

            FeedbackPackage_R feedback;
            feedback.feedback = mm_get_feedback(ctx, guess.guess, solution);
            mm_constrain(matches[player], guess.guess, feedback.feedback);

            if (mm_get_turns(matches[player]) == mm_get_max_guesses(ctx) + 1)
            {
                feedback.lost = true;
                feedback.solution = solution;
            }

            send(sockets[player], &feedback, sizeof(feedback), 0);

            if (mm_is_winning_feedback(ctx, feedback.feedback))
            {
                printf("Player %s won in %d turns\n", nicknames[player], mm_get_turns(matches[player]));
                players_finished[player] = true;
                num_players_finished++;
            }
            else
            {
                if (feedback.lost)
                {
                    printf("Player %s lost\n", nicknames[player]);
                    players_finished[player] = true;
                    num_players_finished++;
                }
            }
        }

        // Send summary of round
        RoundEndPackage_R summary;
        int min_turns = INT16_MAX;
        for (int i = 0; i < num_players; i++)
        {
            if (mm_get_turns(matches[i]) < min_turns)
            {
                min_turns = mm_get_turns(matches[i]);
            }
        }
        for (int i = 0; i < num_players; i++)
        {
            summary.num_turns[i] = mm_get_turns(matches[i]);
            for (int j = 0; j < summary.num_turns[i]; j++)
            {
                summary.guesses[i][j] = mm_get_history_guess(matches[i], j);
                summary.feedbacks[i][j] = mm_get_history_feedback(matches[i], j);
            }
            if (mm_get_turns(matches[i]) == min_turns)
            {
                summary.points[i] = 1;
            }
            else
            {
                summary.points[i] = 0;
            }
        }
        send_all(num_players, sockets, &summary, sizeof(RoundEndPackage_R));
        
        printf("Sent summary. Waiting for ack from all players\n");

        // Wait for all players to acknowledge
        bool ready_mask[MAX_NUM_PLAYERS] = { false };
        int ready_counter = 0;
        while (ready_counter != num_players)
        {
            ReadyPackage_Q ready_package;
            int player = read_next(num_players, sockets, ready_mask, &ready_package, sizeof(ReadyPackage_Q));
            ready_mask[player] = true;
            ready_counter++;
            printf("Player %s ACKed\n", nicknames[player]);
        }

        printf("All players have acked\n");
    }

    printf("Game ended, stopping server.\n");
    for (uint8_t i = 0; i < num_players; i++)
    {
        close(sockets[i]);
    }
}
