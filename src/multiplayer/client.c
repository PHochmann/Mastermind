#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <readline/readline.h>

#include "../mastermind.h"
#include "../util/console.h"
#include "../util/string_builder.h"
#include "../util/string_util.h"
#include "client.h"
#include "protocol.h"

#define RETRY_WAIT_MS 100

typedef struct
{
    int socket;
    PlayerState state;
    bool waiting_for_others;
    const char *const *colors;
    MM_Context *ctx;
} ClientData;

static int connect_to_server(const char *ip, int port, int retries)
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

static void receive_transition(ClientData *data)
{
    PlayerStateTransition_R response;
    recv(data->socket, &response, sizeof(PlayerStateTransition_R), 0);
    data->state = response.new_state;
}

static void send_transition(ClientData *data, PlayerState new_state)
{
    PlayerStateTransition_Q state_change;
    state_change.new_state = new_state;
    send(data->socket, &state_change, sizeof(PlayerStateTransition_Q), 0);
}

static bool handle_transition(ClientData *data)
{
    if (data->state == PLAYER_STATE_CONNECTED)
    {
        RulesPackage_R rules;
        recv(data->socket, &rules, sizeof(RulesPackage_R), 0);
        printf("\n~ ~ Server rules ~ ~\n");
        printf("%d players, %d colors, %d slots, %d max. guesses, %d rounds\n",
               rules.num_players, rules.num_colors, rules.num_slots,
               rules.max_guesses, rules.num_rounds);
        data->ctx = mm_new_ctx(rules.max_guesses, rules.num_slots, rules.num_colors, data->colors);
    }
    else if (data->state == PLAYER_STATE_NAME_PUTTING)
    {
        NicknamePackage_Q name_pkg;
        bool validated = false;
        while (!validated)
        {
            char *nickname = readline_fmt("Enter your nickname (max. %d characters): ", MAX_PLAYER_NAME_BYTES - 1);
            if (nickname == NULL)
            {
                if (abort_game(data))
                {
                    return false;
                }
            }
            else
            {
                if (strlen(nickname) >= MAX_PLAYER_NAME_BYTES)
                {
                    printf("Name too long\n");
                }
                else
                {
                    validated = true;
                    strncpy(name_pkg.name, nickname, MAX_PLAYER_NAME_BYTES);
                }
                free(nickname);
            }
        }

        send_transition(data, PLAYER_STATE_NAME_SENT);
        send(data->socket, &name_pkg, sizeof(NicknamePackage_Q), 0);
    }
    else if ()
    {
        // Todo
    }
    else if (data->state == PLAYER_STATE_ABORTED)
    {
        printf("Game aborted by server.\n");
        return false;
    }
    else if (data->state == PLAYER_STATE_TIMEOUT)
    {
        printf("Timeout\n");
    }
    return true;
}

static bool abort_game(ClientData *data)
{
    while (true)
    {
        char *input = readline("Do you want to abort the game (yN)? ");
        clear_input();
        bool confirmed = false;
        switch (to_lower(input[0]))
        {
        case 'y':
            confirmed = true;
            break;
        case 'n':
            break;
        case '\0':
            break;
        default:
            continue;
        }
        if (!confirmed)
        {
            return false;
        }
        else
        {
            break;
        }
    }
    send_transition(data, PLAYER_STATE_ABORTED);
    return true;
}

void play_client(const char *ip, int port, const char *const *colors)
{
    printf("Trying to connect to server %s:%d...\n", ip, port);
    int socket = connect_to_server(ip, port, -1);
    if (socket == -1)
    {
        return;
    }

    ClientData data = {
        .socket = socket,
        .colors = colors,
        .state  = PLAYER_STATE_CONNECTED
    };

    // Read nickname from player and send it to server
    char *nickname                      = NULL;
    NicknamePackage_R nickname_response = { 0 };
    while (!nickname_response.is_accepted)
    {
        free(nickname);
        nickname = readline_fmt("Enter your nickname (max. %d characters): ", MAX_PLAYER_NAME_BYTES - 1);
        if (nickname == NULL)
        {
            if (abort_game(sock))
            {
                close(sock);
                return;
            }
        }
        else
        {
            if (strlen(nickname) >= MAX_PLAYER_NAME_BYTES)
            {
                printf("Name too long\n");
            }
            else
            {
                NicknamePackage_Q request;
                strncpy(request.name, nickname, MAX_PLAYER_NAME_BYTES);
                send(sock, &request, sizeof(NicknamePackage_Q), 0);
                if (!recv_pkg(sock, &nickname_response, sizeof(NicknamePackage_R)))
                {
                    abort_by_server();
                    close(sock);
                    free(nickname);
                    return;
                }
                if (!nickname_response.is_accepted)
                {
                    printf("Name already taken, please use another.\n");
                }
            }
        }
    }

    if (nickname_response.waiting_for_others)
    {
        printf("Waiting for the other players...\n");
    }

    // Nickname is accepted, wait for names of other players
    AllNicknamesPackage_R names;
    if (!recv_pkg(sock, &names, sizeof(AllNicknamesPackage_R)))
    {
        abort_by_server();
        goto exit;
    }

    for (int i = 0; i < rules.num_rounds; i++)
    {
        printf("Press enter when you're ready for the %s round.", ((i == 0) ? "first" : "next"));
        await_enter();
        ReadyPackage_Q ack;
        send(sock, &ack, sizeof(ReadyPackage_Q), 0);
        ReadyPackage_R answer;
        if (!recv(sock, &answer, sizeof(ReadyPackage_R), 0);
        if (answer.waiting_for_others)
        {
            printf("Waiting for other players...\n");
        }
        RoundBeginPackage_R begin;
        recv(sock, &begin, sizeof(RoundEndPackage_R), 0);

        printf("\n~ ~ Round %d/%d ~ ~\n", i + 1, rules.num_rounds);

        bool finished = false;
        int counter   = 0;
        while (!finished)
        {
            GuessPackage_Q guess_package;
            if (!read_colors(ctx, counter, &guess_package.guess))
            {
                if (abort_game(sock))
                {
                    free(nickname);
                    close(sock);
                    return;
                }
            }
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
        print_round_summary_table(ctx, rules.num_players, names.players,
                                  summary.num_turns, summary.guesses,
                                  summary.feedbacks, i);
        for (int j = 0; j < rules.num_players; j++)
        {
            total_points[j] = summary.points[j];
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

    print_game_summary_table(rules.num_players, names.players, total_points, best_points);
    printf("\n");
exit:
    free(nickname);
    close(sock);
}
