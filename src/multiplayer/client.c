#define _DEFAULT_SOURCE
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

#define RETRY_WAIT_MS 500

typedef struct
{
    int socket;
    PlayerState previous_state;
    PlayerState state;
    const char *const *colors;
    MM_Context *ctx;
    int curr_round;
    int curr_turn;
    AllNicknamesPackage_R names;
    RulesPackage_R rules;
} ClientData;

static int connect_to_server(const char *ip, int port, int retries)
{
    int socket_to_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_to_server < 0)
    {
        printf("Error creating socket\n");
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

    printf("Error connecting to server\n");
    return -1;
}

static void receive_transition(ClientData *data)
{
    StateTransition_RQ response;
    recv(data->socket, &response, sizeof(StateTransition_RQ), 0);
    data->state = response.state;
    printf("Received state %s\n", player_state_to_str(response.state));
}

static void send_transition(ClientData *data, PlayerState state)
{
    StateTransition_RQ state_change = {
        .state = state
    };
    send(data->socket, &state_change, sizeof(StateTransition_RQ), 0);
    data->state = state;
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

static void handle_transition(ClientData *data)
{
    if (data->state == PLAYER_STATE_RULES_RECEIVED)
    {
        RulesPackage_R rules;
        recv(data->socket, &rules, sizeof(RulesPackage_R), 0);
        printf("~ ~ Server rules ~ ~\n"
               "%d players, %d colors, %d slots, %d max. guesses, %d rounds\n\n",
               rules.num_players, rules.num_colors, rules.num_slots, rules.max_guesses, rules.num_rounds);
        data->rules = rules;
        data->ctx   = mm_new_ctx(rules.max_guesses, rules.num_slots, rules.num_colors, data->colors);
    }
    else if (data->state == PLAYER_STATE_CHOOSING_NAME)
    {
        if (data->previous_state == PLAYER_STATE_SENT_NAME)
        {
            printf("The name you entered is already used by another player\n");
        }

        NicknamePackage_Q name_pkg;
        bool validated = false;
        while (!validated)
        {
            char *nickname = readline_fmt("Enter your nickname (max. %d characters): ", MAX_PLAYER_NAME_BYTES - 1);
            if (nickname == NULL)
            {
                if (abort_game(data))
                {
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
                    validated = true;
                    strncpy(name_pkg.name, nickname, MAX_PLAYER_NAME_BYTES);
                }
                free(nickname);
            }
        }

        send_transition(data, PLAYER_STATE_SENT_NAME);
        send(data->socket, &name_pkg, sizeof(NicknamePackage_Q), 0);
    }
    else if (data->state == PLAYER_STATE_NOT_ACKED)
    {
        if (data->curr_round != 0)
        {
            RoundEndPackage_R summary;
            recv(data->socket, &summary, sizeof(RoundEndPackage_R), 0);
            print_round_summary_table(data->ctx, data->rules.num_players, data->names.players,
                                      summary.num_turns, summary.guesses,
                                      summary.feedbacks, data->curr_round);
            if (data->curr_round == data->rules.num_rounds)
            {
                int best_points = 0;
                for (int i = 0; i < data->rules.num_players; i++)
                {
                    if (best_points < summary.points[i])
                    {
                        best_points = summary.points[i];
                    }
                }
                print_game_summary_table(data->rules.num_players, data->names.players, summary.points, best_points);
                return;
            }
        }

        printf("Press enter when you're ready for the %s round.", ((data->curr_round == 0) ? "first" : "next"));
        await_enter();
        send_transition(data, PLAYER_STATE_ACKED);
    }
    else if (data->state == PLAYER_STATE_ACKED)
    {
        printf("Waiting for other players...\n");
    }
    else if (data->state == PLAYER_STATE_GUESSING)
    {
        if (data->curr_round == 0) // Indicates beginning of first round
        {
            AllNicknamesPackage_R all_names;
            recv(data->socket, &all_names, sizeof(AllNicknamesPackage_R), 0);
            data->names = all_names;
        }

        if (data->curr_turn == 0) // Indicates beginning of round
        {
            data->curr_round++;
            printf("\n~ ~ Round %d/%d ~ ~\n", data->curr_round, data->rules.num_rounds);
        }

        data->curr_turn++;

        GuessPackage_Q guess_package;
        bool validated = false;
        while (!validated)
        {
            if (!read_colors(data->ctx, data->curr_turn, &guess_package.guess))
            {
                if (!abort_game(data))
                {
                    return;
                }
            }
            else
            {
                validated = true;
            }
        }

        send_transition(data, PLAYER_STATE_AWAITING_FEEDBACK);
        send(data->socket, &guess_package, sizeof(GuessPackage_Q), 0);
    }
    else if (data->state == PLAYER_STATE_GOT_FEEDBACK)
    {
        FeedbackPackage_R feedback;
        recv(data->socket, &feedback, sizeof(FeedbackPackage_R), 0);
        print_feedback(data->ctx, feedback.feedback);
        printf("\n");

        switch (feedback.match_state)
        {
        case MM_MATCH_LOST:
            printf("~ ~ Game Over! ~ ~\n");
            printf("~ ~ Solution: ");
            print_colors(data->ctx, feedback.solution);
            printf(" ~ ~\n");
            break;
        case MM_MATCH_WON:
            printf("~ ~ You guessed right! ~ ~\n");
            break;
        case MM_MATCH_PENDING:
            break;
        }

        if (feedback.match_state != MM_MATCH_PENDING && feedback.waiting_for_others)
        {
            printf("Waiting for other players to finish the round...\n");
        }
    }
    else if (data->state == PLAYER_STATE_FINISHED)
    {
        data->curr_turn = 0;
    }
    else if (data->state == PLAYER_STATE_ABORTED)
    {
        printf("Game aborted by server.\n");
    }
    else if (data->state == PLAYER_STATE_TIMEOUT)
    {
        printf("Timeout!\n");
    }
    else if (data->state == PLAYER_STATE_DISCONNECTED)
    {
        printf("Disconnected from server. Bye!\n");
    }
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
        .socket     = socket,
        .colors     = colors,
        .state      = PLAYER_STATE_CONNECTED,
        .curr_round = 0
    };

    while (data.state != PLAYER_STATE_DISCONNECTED)
    {
        receive_transition(&data);
        handle_transition(&data);
    }
}
