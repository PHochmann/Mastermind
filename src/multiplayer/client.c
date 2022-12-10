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

bool send_nickname(int fd, const char *nickname)
{
    NicknamePackage_Q request;
    NicknamePackage_R response;
    strncpy(request.name, nickname, MAX_PLAYER_NAME_LENGTH);

    if (send(fd, &request, sizeof(NicknamePackage_Q), 0) < 0)
    {
        return false;
    }
    else if (recv(fd, &response, sizeof(NicknamePackage_R), 0) < 0)
    {
        return false;
    }
    return response.is_accepted;
}

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
        nickname = readline("Enter your nickname (max. 9 characters): ");
        if (strlen(nickname) >= MAX_PLAYER_NAME_LENGTH)
        {
            printf("Name too long\n");
        }
        else
        {
            NicknamePackage_Q request;
            strncpy(request.name, nickname, MAX_PLAYER_NAME_LENGTH);
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

    printf("Server rules:\n");
    printf("Players: ");
    for (int i = 0; i < rules.num_players; i++)
    {
        printf("%s, ", rules.players[i]);
    }
    printf("\n");
    printf("Num players: %d\n", rules.num_players);
    printf("Num colors: %d\n", rules.num_colors);
    printf("Num max guesses: %d\n", rules.max_guesses);
    printf("Num rounds: %d\n", rules.num_rounds);

    MM_Context *ctx = mm_new_ctx(rules.max_guesses, rules.num_slots, rules.num_colors, colors);
    int total_points[MAX_NUM_PLAYERS] = { 0 };

    for (int i = 0; i < rules.num_rounds; i++)
    {
        printf("Waiting for server to start next round...\n");
        RoundBeginPackage_R begin_package;
        recv(sock, &begin_package, sizeof(RoundBeginPackage_R), 0);

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
                printf("~ ~ Waiting for other players to finish... ~ ~\n");
                finished = true;
            }
            else if (mm_is_winning_feedback(ctx, feedback_package.feedback))
            {
                printf("~ ~ You guessed right! Waiting for other players to finish... ~ ~\n");
                finished = true;
            }
        }

        RoundEndPackage_R summary;
        recv(sock, &summary, sizeof(RoundEndPackage_R), 0);
        print_round_summary_table(ctx, rules.num_players, rules.players, summary.num_turns, summary.guesses, summary.feedbacks, i);
        for (int j = 0; j < rules.num_players; j++)
        {
            total_points[j] += summary.points[j];
        }

        any_key();
        ReadyPackage_Q ack;
        send(sock, &ack, sizeof(ReadyPackage_Q), 0);
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
