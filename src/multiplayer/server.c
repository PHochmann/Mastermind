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

#include "../mastermind.h"
#include "../util/console.h"
#include "protocol.h"
#include "server.h"

#define TIMEOUT_SECONDS 100

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

typedef struct
{
    PlayerState previous_state;
    PlayerState state;
    int socket;
    char name[MAX_PLAYER_NAME_BYTES];
    MM_Match *match;
    int points;
} Player;

static void open_listening_socket(ServerData *data)
{
    data->listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data->listening_socket < 0)
    {
        printf("Error creating listening socket.\n");
        return -1;
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
    int bind_code = bind(data->listening_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (bind_code < 0)
    {
        printf("Failed binding address to socket: %d\n", bind_code);
        close(data->listening_socket);
        return -1;
    }

    if (listen(data->listening_socket, data->num_players) < 0)
    {
        printf("Listen failed\n");
        close(data->listening_socket);
        return -1;
    }
    else
    {
        printf("Listening on port %d\n", data->port);
    }
}

static bool send_broadcast(ServerData *data, void *buffer, size_t size)
{
    for (int i = 0; i < data->num_players; i++)
    {
        if (send(data->players[i].socket, buffer, size, 0) < 0)
        {
            return false;
        }
    }
    return true;
}

static int get_next_transition(ServerData *data)
{
    fd_set clients;
    FD_ZERO(&clients);
    for (int i = 0; i < data->num_players; i++)
    {
        if (data->players[i].state != PLAYER_STATE_NOT_CONNECTED)
        {
            FD_SET(data->players[i].socket, &clients);
        }
    }
    if (data->listening_socket != -1)
    {
        FD_SET(data->listening_socket, &clients);
    }

    struct timeval timeout = { TIMEOUT_SECONDS, 0 };
    int sel                = select(FD_SETSIZE, &clients, NULL, NULL, &timeout);
    if (sel == 0)
    {
        for (int i = 0; i < data->num_players; i++)
        {
            if (data->players[i].state == PLAYER_STATE_NOT_CONNECTED
                || data->players[i].state == PLAYER_STATE_NOT_ACKED
                || data->players[i].state == PLAYER_STATE_GUESSING)
            {
                data->players[i].state = PLAYER_STATE_TIMEOUT;
                return i;
            }
        }
    }

    for (int i = 0; i < data->num_players; i++)
    {
        if (FD_ISSET(data->players[i].socket, &clients))
        {
            PlayerStateTransition_Q transition;
            if (recv(data->players[i].socket, &transition, sizeof(PlayerStateTransition_Q), 0) < 0)
            {
                return -1;
            }
            data->players[i].state = transition.new_state;
            return i;
        }
    }

    if (data->listening_socket != -1)
    {
        if (FD_ISSET(data->listening_socket, &clients))
        {
            int new_id = 0;
            while (data->players[new_id].state != PLAYER_STATE_NOT_CONNECTED)
            {
                new_id++;
            }
            data->players[new_id].state = PLAYER_STATE_CONNECTED;
            struct sockaddr_in client_addr;
            unsigned int addr_size;
            data->players[new_id].socket = accept(data->listening_socket, (struct sockaddr *)&client_addr, &addr_size);
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(INET_ADDRSTRLEN * sizeof(char)));
            printf("Accepted client %s (%d/%d)\n", client_ip, new_id + 1, data->num_players);
            return new_id;
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
    PlayerStateTransition_R response = {
        .new_state          = state,
        .waiting_for_others = false
    };
    send_broadcast(data, &response, sizeof(PlayerStateTransition_R));
}

static void send_transition(ServerData *data, Player *player, PlayerState state)
{
    player->state       = state;
    int num_succ_states = 0;
    for (int i = 0; i < data->num_players; i++)
    {
        if (data->players[i].state >= state)
        {
            num_succ_states++;
        }
    }
    PlayerStateTransition_R response = {
        .new_state          = state,
        .waiting_for_others = (num_succ_states < data->num_players)
    };
    send(player->socket, &response, sizeof(PlayerStateTransition_R), 0);
}

static bool handle_transition(ServerData *data, int pl)
{
    Player *player = &data->players[pl];

    if (player->state == PLAYER_STATE_CONNECTED)
    {
        if (pl == data->num_players)
        {
            close(data->listening_socket);
            data->listening_socket = -1;
        }
        RulesPackage_R rules = (RulesPackage_R){
            .num_players = data->num_players,
            .num_rounds  = data->num_rounds,
            .max_guesses = mm_get_max_guesses(data->ctx),
            .num_colors  = mm_get_num_colors(data->ctx),
            .num_slots   = mm_get_num_slots(data->ctx),
        };
        send_transition(data, player, PLAYER_STATE_CONNECTED);
        send(player->socket, &rules, sizeof(RulesPackage_R), 0);
        send_transition(data, player, PLAYER_STATE_NAME_PUTTING);
    }
    else if (player->state == PLAYER_STATE_NAME_SENT)
    {
        NicknamePackage_Q nickname;
        recv(player->socket, &nickname, sizeof(NicknamePackage_Q), 0);
        nickname.name[MAX_PLAYER_NAME_BYTES - 1] = '\0';
        bool duplicate                           = false;
        for (int i = 0; i < data->num_players; i++)
        {
            if (data->players[i].state > PLAYER_STATE_NAME_SENT)
            {
                if (strcmp(nickname.name, data->players[i].name) == 0)
                {
                    duplicate = true;
                    break;
                }
            }
        }
        send_transition(data, player, (duplicate ? PLAYER_STATE_NAME_PUTTING : PLAYER_STATE_NOT_ACKED));
        printf("Got nickname from player[%d]: %s, ", pl, nickname.name);
        if (duplicate)
        {
            printf("duplicate\n");
        }
        else
        {
            printf("accepted\n");
            strncpy(player->name, nickname.name, MAX_PLAYER_NAME_BYTES);
        }
    }
    else if (player->state == PLAYER_STATE_ACKED)
    {
        if (is_all(data, PLAYER_STATE_ACKED))
        {
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
        }
        else
        {
            send_transition(data, player, PLAYER_STATE_ACKED);
        }
    }
    else if (player->state == PLAYER_STATE_AWAITING_FEEDBACK)
    {
        GuessPackage_Q guess;
        recv(player->socket, &guess, sizeof(GuessPackage_Q), 0);
        printf("%s guessed ", player->name);
        print_colors(data->ctx, guess.guess);
        printf("\n");

        FeedbackPackage_R feedback = { 0 };
        feedback.feedback          = mm_get_feedback(data->ctx, guess.guess, data->curr_solution);
        mm_constrain(player->match, guess.guess, feedback.feedback);

        switch (mm_get_state(player->match))
        {
        case MM_MATCH_PENDING:
            send_transition(data, player, PLAYER_STATE_GUESSING);
            break;
        case MM_MATCH_WON:
            send_transition(data, player, PLAYER_STATE_FINISHED);
            break;
        case MM_MATCH_LOST:
            send_transition(data, player, PLAYER_STATE_FINISHED);
            feedback.solution = data->curr_solution;
            break;
        }

        send(player->socket, &feedback, sizeof(FeedbackPackage_R), 0);

        if (player->state == PLAYER_STATE_FINISHED)
        {
            printf("%s ended in %d turns\n", player->name, mm_get_turns(player->match));
        }

        if (is_all(data, PLAYER_STATE_FINISHED))
        {
            printf("Round ended\n");
            RoundEndPackage_R summary = { 0 };
            int min_turns             = INT16_MAX;

            for (int i = 0; i < data->num_players; i++)
            {
                summary.num_turns[i] = mm_get_turns(data->players[i].match);
                if (summary.num_turns[i] < min_turns)
                {
                    min_turns = summary.num_turns[i];
                }
            }

            for (int i = 0; i < data->num_players; i++)
            {
                for (int j = 0; j < summary.num_turns[i]; j++)
                {
                    summary.guesses[i][j]   = mm_get_history_guess(data->players[i].match, j);
                    summary.feedbacks[i][j] = mm_get_history_feedback(data->players[i].match, j);
                }
                if ((summary.num_turns[i] == min_turns) && (mm_get_state(data->players[i].match) == MM_MATCH_WON))
                {
                    data->players[i].points++;
                    summary.points[i] = data->players[i].points;
                }
            }
            send_transition_broadcast(data, PLAYER_STATE_NOT_ACKED);
            send_broadcast(data, &summary, sizeof(RoundEndPackage_R));
        }
    }
    else if (player->state == PLAYER_STATE_ABORTED)
    {
        printf("Game aborted by %s\n", player->name);
        send_transition_broadcast(data, PLAYER_STATE_ABORTED);
        return false;
    }
    else if (player->state == PLAYER_STATE_TIMEOUT)
    {
        printf("Timeout of player %s", player->name);
        send_transition(data, player, PLAYER_STATE_TIMEOUT);
        send_transition_broadcast(data, PLAYER_STATE_ABORTED);
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
    ServerData data = {
        .listening_socket = -1,
        .port             = port,
        .num_players      = num_players,
        .num_rounds       = num_rounds,
        .ctx              = ctx
    };
    for (int i = 0; i < num_players; i++)
    {
        data.players[i].state = PLAYER_STATE_NOT_CONNECTED;
    }

    open_listening_socket(&data);
    if (data.listening_socket < 0)
    {
        return;
    }

    for (int i = 0; i < num_rounds; i++)
    {
        data.curr_solution = rand() & mm_get_num_codes(ctx);
        data.curr_round    = i;

        while (!is_all(&data, PLAYER_STATE_ACKED))
        {
            int pl = get_next_transition(&data);
            if (!handle_transition(&data, pl))
            {
                goto exit;
            }
        }

        while (!is_all(&data, PLAYER_STATE_FINISHED))
        {
            int pl = get_next_transition(&data);
            if (!handle_transition(&data, pl))
            {
                goto exit;
            }
        }
    }

    printf("Game ended, stopping server.\n");
exit:
    if (data.listening_socket != -1)
    {
        close(data.listening_socket);
    }
    for (int i = 0; i < num_players; i++)
    {
        close(data.players[i].socket);
    }
}
