#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <readline/readline.h>
#include <signal.h>

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
    MM_Context *ctx;
    int curr_round;
    int curr_turn;
    AllNicknamesPackage_R names;
    RulesPackage_R rules;
    bool ended_gracefully;
} ClientData;

static const Transition allowed_recv_transitions[] = {
    { PLAYER_STATE_CONNECTED, PLAYER_STATE_RULES_RECEIVED },
    { PLAYER_STATE_RULES_RECEIVED, PLAYER_STATE_CHOOSING_NAME },
    { PLAYER_STATE_SENT_NAME, PLAYER_STATE_CHOOSING_NAME },
    { PLAYER_STATE_SENT_NAME, PLAYER_STATE_NOT_ACKED },
    { PLAYER_STATE_ACKED, PLAYER_STATE_GUESSING },
    { PLAYER_STATE_ACKED, PLAYER_STATE_ACKED },
    { PLAYER_STATE_ACKED, PLAYER_STATE_FINISHED },
    { PLAYER_STATE_AWAITING_FEEDBACK, PLAYER_STATE_GOT_FEEDBACK },
    { PLAYER_STATE_GOT_FEEDBACK, PLAYER_STATE_GUESSING },
    { PLAYER_STATE_GOT_FEEDBACK, PLAYER_STATE_FINISHED },
    { PLAYER_STATE_FINISHED, PLAYER_STATE_NOT_ACKED },
    { PLAYER_STATE_FINISHED, PLAYER_STATE_DISCONNECTED },
    { PLAYER_STATE_ABORTED, PLAYER_STATE_DISCONNECTED },
};

static const int num_allowed_recv_transitions = sizeof(allowed_recv_transitions) / sizeof(Transition);

bool sigint;
bool sigpipe;

static void log_transition(PlayerState from, PlayerState to)
{
#if defined(DEBUG_TRANSITION_LOG)
    printf("Transition received: %s -> %s.\n", plstate_to_str(from), plstate_to_str(to));
#else
    (void)from;
    (void)to;
#endif
}

static bool receive(int socket, void *buffer, size_t length)
{
    if ((recv(socket, buffer, length, MSG_WAITALL) < (ssize_t)length) || sigpipe)
    {
        return false;
    }
    return true;
}

static void sigint_handler(int signum)
{
    if (signum == SIGINT)
    {
        sigint = true;
    }
}

static void sigpipe_handler(int signum)
{
    if (signum == SIGPIPE)
    {
        sigpipe = true;
    }
}

static void close_sockets(ClientData *data)
{
    close(data->socket);
}

static void await_enter_abortable()
{
    while (getchar() != '\n' || sigint)
        ;
}

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
        if (connect(socket_to_server, (struct sockaddr *)&server_ip, sizeof(struct sockaddr_in)) != 0)
        {
            if (retries > 0)
            {
                retries--;
            }
            if (sigint || (usleep(RETRY_WAIT_MS * 1000) == -1))
            {
                return -1;
            }
        }
        else
        {
            return socket_to_server;
        }
    }

    printf("Error connecting to server\n");
    return -1;
}

static bool receive_transition(ClientData *data)
{
    StateTransition_RQ response;
    if (!receive(data->socket, &response, sizeof(StateTransition_RQ)))
    {
        response.state = PLAYER_STATE_DISCONNECTED;
    }

    bool legal_transition = false;
    for (int i = 0; i < num_allowed_recv_transitions; i++)
    {
        if ((allowed_recv_transitions[i].from == data->state)
            && (allowed_recv_transitions[i].to == response.state))
        {
            legal_transition = true;
            break;
        }
    }
    if (legal_transition
        || (response.state == PLAYER_STATE_ABORTED)
        || (response.state == PLAYER_STATE_DISCONNECTED))
    {
        data->previous_state = data->state;
        data->state          = response.state;
        log_transition(data->previous_state, data->state);
        return true;
    }
    else
    {
        printf("Illegal transition received: %s -> %s.\n", plstate_to_str(data->state), plstate_to_str(response.state));
        return false;
    }
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
        char *input = readline("Do you want to abort the game? [yN] ");
        clear_input();
        if (input == NULL)
        {
            return false;
        }
        bool confirmed = false;
        switch (to_lower(input[0]))
        {
        case 'y':
            confirmed = true;
            break;
        case 'n':
        case '\0':
        case '\n':
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
        receive(data->socket, &rules, sizeof(RulesPackage_R));
        printf("~ ~ Server rules colors~ ~\n"
               "%d players, %d colors, %d slots, %d max. guesses, %d rounds\n",
               rules.num_players, rules.num_colors, rules.num_slots, rules.max_guesses, rules.num_rounds);
        data->rules = rules;
        data->ctx   = mm_new_ctx(rules.max_guesses, rules.num_slots, rules.num_colors);
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
                clear_input();
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
            receive(data->socket, &summary, sizeof(RoundEndPackage_R));
            print_round_summary_table(data->ctx, data->rules.num_players, data->names.players,
                                      summary.num_turns, summary.guesses,
                                      summary.solution, data->curr_round, summary.points);

            if (summary.winner_pl != -1)
            {
                printf("%s won this round%s.\n",
                       (summary.winner_pl == data->rules.player_id) ? "You" : data->names.players[summary.winner_pl],
                       (summary.win_reason_quicker ? " due to finishing first" : ""));
            }
            else
            {
                printf("Nobody won this round.\n");
            }

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

                int num_winners = 0;
                for (int i = 0; i < data->rules.num_players; i++)
                {
                    if (summary.points[i] == best_points)
                    {
                        num_winners++;
                    }
                }

                printf("\n~ ~ End of game ~ ~\n");
                if (data->rules.num_players > 1)
                {
                    if (num_winners != 1)
                    {
                        printf("It's a draw between ");
                        for (int i = 0; i < data->rules.num_players; i++)
                        {
                            if (summary.points[i] == best_points)
                            {
                                printf("%s, ", data->names.players[i]);
                            }
                        }
                        printf("\n");
                    }
                    else
                    {
                        for (int i = 0; i < data->rules.num_players; i++)
                        {
                            if (summary.points[i] == best_points)
                            {
                                printf("%s won!\n",
                                    (summary.winner_pl == data->rules.player_id) ? "You" : data->names.players[summary.winner_pl]);
                            }
                        }
                    }
                }
                data->ended_gracefully = true;
                return;
            }
        }

        printf("Press enter when you're ready for the %s round.", ((data->curr_round == 0) ? "first" : "next"));
        await_enter_abortable();
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
            receive(data->socket, &all_names, sizeof(AllNicknamesPackage_R));
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
                clear_input();
                if (abort_game(data))
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
        if (!receive(data->socket, &feedback, sizeof(FeedbackPackage_R)))
        {
            return;
        }
        print_feedback(data->ctx, feedback.feedback);
        printf("\n");

        switch (feedback.match_state)
        {
        case MM_MATCH_LOST:
            printf("~ ~ Game Over! You did not make it in %d turns. ~ ~\n", data->rules.max_guesses);
            printf("~ ~ Solution: ");
            print_colors(data->ctx, feedback.solution);
            printf(" ~ ~\n");
            break;
        case MM_MATCH_WON:
            printf("~ ~ You guessed right! You took %d turns. ~ ~\n", data->curr_turn);
            break;
        case MM_MATCH_PENDING:
            break;
        }

        if ((feedback.match_state != MM_MATCH_PENDING) && (feedback.waiting_for_others))
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
        printf("Game aborted.\n");
    }
    else if (data->state == PLAYER_STATE_DISCONNECTED)
    {
        if (!data->ended_gracefully)
        {
            printf("Server closed connection unexpectedly.\n");
        }
    }
}

void play_client(const char *ip, int port)
{
    printf("Trying to connect to server %s:%d...\n", ip, port);
    clear_input();
    sigint  = false;
    sigpipe = false;
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);
    int socket = connect_to_server(ip, port, -1);
    if (socket == -1)
    {
        return;
    }

    ClientData data = {
        .socket = socket,
        .state  = PLAYER_STATE_CONNECTED
    };

    while (data.state != PLAYER_STATE_DISCONNECTED)
    {
        if (!sigint)
        {
            if (receive_transition(&data))
            {
                handle_transition(&data);
            }
        }
        else
        {
            send_transition(&data, PLAYER_STATE_ABORTED);
            break;
        }
    }

    signal(SIGINT, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    close_sockets(&data);
}
