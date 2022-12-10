#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "client.h"
#include "protocol.h"
#include "network.h"
#include "../mastermind.h"
#include "../util/string_builder.h"
#include "../util/console.h"

void play_multiplayer(const char *ip, int port, const char * const * colors)
{
    printf("Trying to connect to server %s:%d...\n", ip, port);
    int sock = connect_to_server(ip, port, -1);
    if (sock == -1) return;
    printf("Successfully connected to server!\n");

    // Request nickname
    char *nickname = NULL;
    NicknamePackage_R nickname_response = { 0 };
    while (!nickname_response.is_accepted)
    {
        free(nickname);
        nickname = readline_fmt("Enter your nickname (max. %d characters): ", MAX_PLAYER_NAME_BYTES - 1);
        if (strlen(nickname) >= MAX_PLAYER_NAME_BYTES)
        {
            printf("Name too long\n");
        }
        else
        {
            NicknamePackage_Q request;
            strncpy(request.name, nickname, MAX_PLAYER_NAME_BYTES);
            send(sock, &request, sizeof(NicknamePackage_Q), 0);
            recv(sock, &nickname_response, sizeof(NicknamePackage_R), 0);

            if (!nickname_response.is_accepted)
            {
                printf("Name already taken, please use another.\n");
            }
        }
    }

    if (nickname_response.waiting_for_others)
    {
        printf("Waiting for the other players...\n");
    }

    // Nickname is accepted, wait for rules
    GameBeginPackage_R rules;
    recv(sock, &rules, sizeof(GameBeginPackage_R), 0);

    // We have received the rules, display them:

    printf("~ ~ Server rules ~ ~\n");
    printf("%d players, %d colors, %d slots, %d max. guesses, %d rounds\n",
        rules.num_players, rules.num_colors, rules.num_slots, rules.max_guesses, rules.num_rounds);

    MM_Context *ctx = mm_new_ctx(rules.max_guesses, rules.num_slots, rules.num_colors, colors);
    int total_points[MAX_NUM_PLAYERS] = { 0 };

    for (int i = 0; i < rules.num_rounds; i++)
    {
        any_key();
        ReadyPackage_Q ack;
        send(sock, &ack, sizeof(ReadyPackage_Q), 0);
        ReadyPackage_R answer;
        recv(sock, &answer, sizeof(ReadyPackage_R), 0);
        if (answer.waiting_for_others)
        {
            printf("Waiting for other players...\n");
        }
        RoundBeginPackage_R begin;
        recv(sock, &begin, sizeof(RoundEndPackage_R), 0);

        printf("\n~ ~ Round %d/%d ~ ~\n", i + 1, rules.num_rounds);

        bool finished = false;
        int counter = 0;
        while (!finished)
        {
            GuessPackage_Q guess_package;
            guess_package.guess = read_colors(ctx, counter);
            counter++;
            send(sock, &guess_package, sizeof(GuessPackage_Q), 0);

            FeedbackPackage_R feedback_package;
            recv(sock, &feedback_package, sizeof(FeedbackPackage_R), 0);
            print_feedback(ctx, feedback_package.feedback);
            printf("\n");

            if (feedback_package.lost)
            {
                printf("~ ~ Game Over! ~ ~\n");
                printf("~ ~ Solution: ");
                print_colors(ctx, feedback_package.solution);
                printf(" ~ ~\n");
                finished = true;
            }
            else if (mm_is_winning_feedback(ctx, feedback_package.feedback))
            {
                printf("~ ~ You guessed right! ~ ~\n");
                finished = true;
            }

            if (finished && feedback_package.waiting_for_others)
            {
                printf("Waiting for all players to finish...\n");
            }
        }

        RoundEndPackage_R summary;
        recv(sock, &summary, sizeof(RoundEndPackage_R), 0);
        print_round_summary_table(ctx, rules.num_players, rules.players, summary.num_turns, summary.guesses, summary.feedbacks, i);
        for (int j = 0; j < rules.num_players; j++)
        {
            total_points[j] += summary.points[j];
        }
    }

    int best_points = 0;
    for (int i = 0; i < rules.num_players; i++)
    {
        if (best_points < total_points[i])
        {
            best_points = total_points[i];
        }
    }

    print_game_summary_table(rules.num_players, rules.players, total_points, best_points);
    printf("\n");
    free(nickname);
    close(sock);
}
