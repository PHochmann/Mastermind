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
    int sockets[MAX_NUM_PLAYERS];
    if (!accept_clients(port, num_players, sockets))
    {
        return;
    }
    printf("All players connected. Validating nicknames... \n");

    bool nickname_validated[MAX_NUM_PLAYERS];
    char nicknames[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES];
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
        response.waiting_for_others = (nickname_validated_count < num_players - 1);
        send(sockets[player], &response, sizeof(NicknamePackage_R), 0);
        printf("Got nickname from player[%d]: %s, ", player, next_nickname.name);
        if (duplicate)
        {
            printf("duplicate\n");
        }
        else
        {
            printf("accepted\n");
            strncpy(nicknames[player], next_nickname.name, MAX_PLAYER_NAME_BYTES);
            nickname_validated_count++;
        }
    }

    printf("All players have nicknames. Sending server rules.\n");
    GameBeginPackage_R begin_package = (GameBeginPackage_R){
        .num_players = num_players,
        .num_rounds = num_rounds,
        .max_guesses = mm_get_max_guesses(ctx),
        .num_colors = mm_get_num_colors(ctx),
        .num_slots = mm_get_num_slots(ctx),
    };
    for (int i = 0; i < num_players; i++)
    {
        strncpy(begin_package.players[i], nicknames[i], MAX_PLAYER_NAME_BYTES);
    }
    send_all(num_players, sockets, &begin_package, sizeof(GameBeginPackage_R));

    for (int i = 0; i < num_rounds; i++)
    {
        printf("Waiting for ACK from all players\n");
        bool ready_mask[MAX_NUM_PLAYERS] = { false };
        int ready_counter = 0;
        while (ready_counter != num_players)
        {
            ReadyPackage_Q ready_package;
            int player = read_next(num_players, sockets, ready_mask, &ready_package, sizeof(ReadyPackage_Q));
            ready_mask[player] = true;
            ready_counter++;
            printf("Player %s ACKed\n", nicknames[player]);

            ReadyPackage_R ready_answer;
            ready_answer.waiting_for_others = (ready_counter < num_players);
            send(sockets[player], &ready_answer, sizeof(ReadyPackage_R), 0);
        }

        RoundBeginPackage_R begin_package;
        send_all(num_players, sockets, &begin_package, sizeof(RoundBeginPackage_R));

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

            printf("%s guessed ", nicknames[player]);
            print_colors(ctx, guess.guess);
            printf("\n");

            FeedbackPackage_R feedback = { 0 };
            feedback.feedback = mm_get_feedback(ctx, guess.guess, solution);
            mm_constrain(matches[player], guess.guess, feedback.feedback);
            bool ends_round = false;

            if (mm_is_winning_feedback(ctx, feedback.feedback))
            {
                ends_round = true;
            }
            else
            {
                if (mm_get_turns(matches[player]) == mm_get_max_guesses(ctx))
                {
                    ends_round = true;
                    feedback.lost = true;
                    feedback.solution = solution;
                }
            }

            if (ends_round)
            {
                printf("%s ended in %d turns\n", nicknames[player], mm_get_turns(matches[player]));
                players_finished[player] = true;
                num_players_finished++;

                if (num_players_finished < num_players)
                {
                    feedback.waiting_for_others = true;
                }
            }
            send(sockets[player], &feedback, sizeof(FeedbackPackage_R), 0);
        }

        // Send summary of round
        RoundEndPackage_R summary = { 0 };
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
        }
        send_all(num_players, sockets, &summary, sizeof(RoundEndPackage_R));
    }

    printf("Game ended, stopping server.\n");
    for (uint8_t i = 0; i < num_players; i++)
    {
        close(sockets[i]);
    }
}
