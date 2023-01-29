#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>

#include "../mastermind.h"
#include "../util/console.h"
#include "../util/string_util.h"
#include "protocol.h"
#include "server.h"

typedef struct
{
    PlayerState previous_state;
    PlayerState state;
    int socket;
    char name[MAX_PLAYER_NAME_BYTES];
    MM_Match *match;
    int points;
    int position;
} Player;

typedef struct
{
    int port;
    int listening_socket;
    int num_players;
    int num_rounds;
    int curr_round;
    Player players[MAX_NUM_PLAYERS];
    MM_Context *ctx;
    Code_t curr_solution;
} ServerData;

static const Transition allowed_recv_transitions[] = {
    // Normal behaviour:
    { PLAYER_STATE_NONE, PLAYER_STATE_CONNECTED },
    { PLAYER_STATE_CHOOSING_NAME, PLAYER_STATE_SENT_NAME },
    { PLAYER_STATE_NOT_ACKED, PLAYER_STATE_ACKED },
    { PLAYER_STATE_GUESSING, PLAYER_STATE_AWAITING_FEEDBACK },
    // Errors:
    { PLAYER_STATE_CHOOSING_NAME, PLAYER_STATE_ABORTED },
    { PLAYER_STATE_NOT_ACKED, PLAYER_STATE_ABORTED },
    { PLAYER_STATE_GUESSING, PLAYER_STATE_ABORTED }
};

static const int num_allowed_recv_transitions = sizeof(allowed_recv_transitions) / sizeof(Transition);

static bool sigint;
static bool sigpipe;

static void log_transition(PlayerState from, PlayerState to)
{
#if defined(DEBUG_TRANSITION_LOG)
    printf("Transition received: %s -> %s.\n", plstate_to_str(from), plstate_to_str(to));
#else
    (void)from;
    (void)to;
#endif
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

static void close_sockets(ServerData *data)
{
    if (data->listening_socket != -1)
    {
        close(data->listening_socket);
    }
    for (int i = 0; i < data->num_players; i++)
    {
        close(data->players[i].socket);
    }
}

static void open_listening_socket(ServerData *data)
{
    data->listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data->listening_socket < 0)
    {
        printf("Error creating listening socket.\n");
    }

    struct in_addr ip;             // Create IP
    ip.s_addr = htonl(INADDR_ANY); // Use any IP
    struct sockaddr_in sockaddr;   // Struct for socket address for TCP/IP
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port   = htons(data->port);
    sockaddr.sin_addr   = ip;

    // Binds address to server's socket
    // Client calls connect() on the same address
    // Signature of bind() and connect() is the same
    if (bind(data->listening_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
    {
        printf("Failed binding address to socket\n");
        close(data->listening_socket);
        data->listening_socket = -1;
        return;
    }

    if (listen(data->listening_socket, data->num_players) < 0)
    {
        printf("Listen failed\n");
        close(data->listening_socket);
        data->listening_socket = -1;
        return;
    }
    else
    {
        printf("Listening on port %d\n", data->port);
    }
}

static void send_broadcast(ServerData *data, void *buffer, size_t size)
{
    for (int i = 0; i < data->num_players; i++)
    {
        if (data->players[i].state != PLAYER_STATE_NONE)
        {
            send(data->players[i].socket, buffer, size, 0);
        }
    }
}

static int get_next_transition(ServerData *data, PlayerState *out_state)
{
    fd_set clients;
    FD_ZERO(&clients);
    for (int i = 0; i < data->num_players; i++)
    {
        if (data->players[i].state != PLAYER_STATE_NONE)
        {
            FD_SET(data->players[i].socket, &clients);
        }
    }
    if (data->listening_socket != -1)
    {
        FD_SET(data->listening_socket, &clients);
    }

    select(FD_SETSIZE, &clients, NULL, NULL, NULL);
    if (sigpipe || sigint)
    {
        return -1;
    }

    if (data->listening_socket != -1)
    {
        if (FD_ISSET(data->listening_socket, &clients))
        {
            int new_id = 0;
            while ((data->players[new_id].state != PLAYER_STATE_NONE) && (new_id < data->num_players))
            {
                new_id++;
            }
            struct sockaddr_in client_addr;
            unsigned int addr_size;
            data->players[new_id].socket = accept(data->listening_socket, (struct sockaddr *)&client_addr, &addr_size);
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, INET_ADDRSTRLEN * sizeof(char));
            printf("Accepted client %s (%d/%d)\n", client_ip, new_id + 1, data->num_players);
            *out_state = PLAYER_STATE_CONNECTED;
            if ((new_id + 1) == data->num_players)
            {
                close(data->listening_socket);
                data->listening_socket = -1;
            }

            return new_id;
        }
    }

    for (int i = 0; i < data->num_players; i++)
    {
        if (FD_ISSET(data->players[i].socket, &clients))
        {
            StateTransition_RQ transition;
            recv(data->players[i].socket, &transition, sizeof(StateTransition_RQ), MSG_WAITALL);
            *out_state = transition.state;
            return i;
        }
    }

    return -1;
}

static int count_states(ServerData *data, PlayerState state)
{
    int res = 0;
    for (int i = 0; i < data->num_players; i++)
    {
        if (data->players[i].state == state)
        {
            res++;
        }
    }
    return res;
}

static bool is_all(ServerData *data, PlayerState state)
{

    return (count_states(data, state) == data->num_players);
}

static void send_transition_broadcast(ServerData *data, PlayerState state)
{
    StateTransition_RQ response = {
        .state = state
    };
    for (int i = 0; i < data->num_players; i++)
    {
        data->players[i].state = state;
    }
    send_broadcast(data, &response, sizeof(StateTransition_RQ));
}

static void send_transition(Player *player, PlayerState state)
{
    player->previous_state      = player->state;
    player->state               = state;
    StateTransition_RQ response = {
        .state = state
    };
    send(player->socket, &response, sizeof(StateTransition_RQ), 0);
}

static void handle_transition(ServerData *data, int pl)
{
    Player *player = &data->players[pl];

    if (player->state == PLAYER_STATE_CONNECTED)
    {
        RulesPackage_R rules = (RulesPackage_R){
            .player_id   = pl,
            .num_players = data->num_players,
            .num_rounds  = data->num_rounds,
            .max_guesses = mm_get_max_guesses(data->ctx),
            .num_colors  = mm_get_num_colors(data->ctx),
            .num_slots   = mm_get_num_slots(data->ctx),
        };
        send_transition(player, PLAYER_STATE_RULES_RECEIVED);
        send(player->socket, &rules, sizeof(RulesPackage_R), 0);
        send_transition(player, PLAYER_STATE_CHOOSING_NAME);
    }
    else if (player->state == PLAYER_STATE_SENT_NAME)
    {
        NicknamePackage_Q nickname;
        recv(player->socket, &nickname, sizeof(NicknamePackage_Q), MSG_WAITALL);
        nickname.name[MAX_PLAYER_NAME_BYTES - 1] = '\0';
        bool rejected                            = false;
        if (strlen(nickname.name) == 0)
        {
            rejected = true;
        }
        for (int i = 0; i < data->num_players; i++)
        {
            if (data->players[i].state > PLAYER_STATE_SENT_NAME)
            {
                if (strcmp(nickname.name, data->players[i].name) == 0)
                {
                    rejected = true;
                    break;
                }
            }
        }
        send_transition(player, (rejected ? PLAYER_STATE_CHOOSING_NAME : PLAYER_STATE_NOT_ACKED));
        printf("Got nickname from player[%d]: %s, ", pl, nickname.name);
        if (rejected)
        {
            printf("rejected\n");
        }
        else
        {
            printf("accepted\n");
            strncpy(player->name, nickname.name, MAX_PLAYER_NAME_BYTES);
        }
    }
    else if (player->state == PLAYER_STATE_ACKED)
    {
        printf("%s ACKed (%d/%d)\n", player->name, count_states(data, PLAYER_STATE_ACKED), data->num_players);

        if (is_all(data, PLAYER_STATE_ACKED)) // Start round
        {
            for (int i = 0; i < data->num_players; i++)
            {
                data->players[i].match  = mm_new_match(data->ctx, false);
                data->players->position = INT16_MAX;
            }
            data->curr_solution = rand() % mm_get_num_codes(data->ctx);
            send_transition_broadcast(data, PLAYER_STATE_GUESSING);
            if (data->curr_round == 0)
            {
                AllNicknamesPackage_R all_names;
                for (int i = 0; i < data->num_players; i++)
                {
                    strncpy(all_names.players[i], data->players[i].name, MAX_PLAYER_NAME_BYTES);
                }
                send_broadcast(data, &all_names, sizeof(AllNicknamesPackage_R));
            }
            data->curr_round++;
            printf("Start of round %d of %d\n", data->curr_round, data->num_rounds);
            printf("Solution: ");
            print_colors(data->ctx, data->curr_solution);
            printf("\n");
        }
        else
        {
            send_transition(player, PLAYER_STATE_ACKED);
        }
    }
    else if (player->state == PLAYER_STATE_AWAITING_FEEDBACK)
    {
        GuessPackage_Q guess;
        recv(player->socket, &guess, sizeof(GuessPackage_Q), MSG_WAITALL);

        FeedbackPackage_R feedback = { 0 };
        feedback.feedback          = mm_get_feedback(data->ctx, guess.guess, data->curr_solution);
        mm_constrain(player->match, guess.guess, feedback.feedback);
        feedback.match_state = mm_get_state(player->match);
        if (feedback.match_state == MM_MATCH_LOST)
        {
            feedback.solution = data->curr_solution;
        }
        int rdy_counter = 0;
        for (int i = 0; i < data->num_players; i++)
        {
            if ((i == pl) || (data->players[i].state == PLAYER_STATE_FINISHED))
            {
                rdy_counter++;
            }
        }
        feedback.waiting_for_others = (rdy_counter < data->num_players);
        send_transition(player, PLAYER_STATE_GOT_FEEDBACK);
        send(player->socket, &feedback, sizeof(FeedbackPackage_R), 0);

        switch (mm_get_state(player->match))
        {
        case MM_MATCH_PENDING:
            send_transition(player, PLAYER_STATE_GUESSING);
            break;
        case MM_MATCH_WON:
            send_transition(player, PLAYER_STATE_FINISHED);
            player->position = count_states(data, PLAYER_STATE_FINISHED);
            break;
        case MM_MATCH_LOST:
            send_transition(player, PLAYER_STATE_FINISHED);
            feedback.solution = data->curr_solution;
            break;
        }

        printf("%s guessed ", player->name);
        print_colors(data->ctx, guess.guess);
        printf(" (%d)\n", mm_get_turns(player->match));

        if (player->state == PLAYER_STATE_FINISHED)
        {
            printf("%s ended in %d turns (%d/%d)\n",
                   player->name,
                   mm_get_turns(player->match),
                   count_states(data, PLAYER_STATE_FINISHED),
                   data->num_players);
        }

        if (is_all(data, PLAYER_STATE_FINISHED)) // End round
        {
            RoundEndPackage_R summary = { 0 };
            summary.solution          = data->curr_solution;
            summary.winner_pl         = -1;
            for (int i = 0; i < data->num_players; i++)
            {
                summary.num_turns[i] = mm_get_turns(data->players[i].match);
                summary.points[i]    = data->players[i].points;
                for (int j = 0; j < summary.num_turns[i]; j++)
                {
                    summary.guesses[i][j] = mm_get_history_guess(data->players[i].match, j);
                }

                if (mm_get_state(data->players[i].match) == MM_MATCH_WON)
                {
                    if ((summary.winner_pl != -1) && (summary.num_turns[i] == summary.num_turns[summary.winner_pl]))
                    {
                        summary.win_reason_quicker = true;
                    }
                    else
                    {
                        summary.win_reason_quicker = false;
                    }

                    if ((summary.num_turns[i] < summary.num_turns[summary.winner_pl])
                        || ((summary.num_turns[i] == summary.num_turns[summary.winner_pl])
                            && (data->players[i].position < data->players[summary.winner_pl].position))
                        || (summary.winner_pl == -1))
                    {
                        summary.winner_pl = i;
                    }
                }

                mm_free_match(data->players[i].match);
            }

            if (summary.winner_pl != -1)
            {
                summary.points[summary.winner_pl]++;
                data->players[summary.winner_pl].points++;
                printf("Round ended. %s won.\n", data->players[summary.winner_pl].name);
            }
            else
            {
                printf("Round ended. Nobody won.\n");
            }

            send_transition_broadcast(data, PLAYER_STATE_NOT_ACKED);
            send_broadcast(data, &summary, sizeof(RoundEndPackage_R));

            if (data->curr_round == data->num_rounds)
            {
                send_transition_broadcast(data, PLAYER_STATE_DISCONNECTED);
            }
        }
    }
    else if (player->state == PLAYER_STATE_ABORTED)
    {
        printf("Game aborted by %s\n", player->name);
        send_transition_broadcast(data, PLAYER_STATE_ABORTED);
    }
}

static bool process_next_transition(ServerData *data)
{
    PlayerState next_state;
    int pl = get_next_transition(data, &next_state);
    if (pl == -1)
    {
        return false;
    }
    bool legal_transition = false;
    for (int i = 0; i < num_allowed_recv_transitions; i++)
    {
        if ((allowed_recv_transitions[i].from == data->players[pl].state)
            && (allowed_recv_transitions[i].to == next_state))
        {
            legal_transition = true;
            break;
        }
    }
    if (legal_transition)
    {
        data->players[pl].previous_state = data->players[pl].state;
        data->players[pl].state          = next_state;
        log_transition(data->players[pl].previous_state, next_state);
        handle_transition(data, pl);
    }
    else
    {
        printf("Illegal transition received: %s -> %s.\n", plstate_to_str(data->players[pl].state), plstate_to_str(next_state));
        return false;
    }
    return true;
}

void start_server(MM_Context *ctx, int num_players, int num_rounds, int port)
{
    if (num_players > MAX_NUM_PLAYERS || num_rounds > MAX_NUM_ROUNDS)
    {
        printf("start_server called with invalid arguments\n");
        return;
    }

    sigint  = false;
    sigpipe = false;
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);

    ServerData data = {
        .listening_socket = -1,
        .port             = port,
        .num_players      = num_players,
        .num_rounds       = num_rounds,
        .ctx              = ctx
    };

    for (int i = 0; i < num_players; i++)
    {
        data.players[i].state  = PLAYER_STATE_NONE;
        data.players[i].socket = -1;
        strncpy(data.players[i].name, "(no name yet)", MAX_PLAYER_NAME_BYTES);
        data.players[i].name[MAX_PLAYER_NAME_BYTES - 1] = '\0';
    }

    open_listening_socket(&data);
    if (data.listening_socket < 0)
    {
        return;
    }

    while (!is_all(&data, PLAYER_STATE_DISCONNECTED))
    {
        if (!process_next_transition(&data))
        {
            send_transition_broadcast(&data, PLAYER_STATE_ABORTED);
            break;
        }
    }

    printf("Stopping server.\n");
    signal(SIGINT, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    close_sockets(&data);
}
