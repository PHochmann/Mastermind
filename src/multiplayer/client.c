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

bool receive_rules(int fd, GameBeginPackage_R *rules)
{
    if (recv(fd, rules, sizeof(GameBeginPackage_R), 0) < 0)
    {
        return false;
    }
    return true;
}

void play_multiplayer(const char *ip, int port)
{
    int sock = connect_to_server(ip, port);
    if (sock == -1) return;
    printf("Successfully connected to server!\n");

    // Request nickname
    char *nickname = NULL;
    bool validated = false;
    while (!validated)
    {
        free(nickname);
        nickname = readline("Enter your nickname (max. 9 characters): ");
        if (strlen(nickname) >= MAX_PLAYER_NAME_LENGTH)
        {
            printf("Name too long\n");
            validated = false;
        }
        else if (!send_nickname(sock, nickname))
        {
            printf("Name already taken, please use another.\n");
            validated = false;
        }
        else
        {
            validated = true;
        }
    }

    printf("Waiting for the other players...\n");

    // Nickname is accepted, wait for rules
    GameBeginPackage_R rules;
    if (!receive_rules(sock, &rules))
    {
        printf("Communication error\n");
        goto exit;
    }

    // We have received the rules, display them:
    printf("Server rules:\n");
    printf("Num players: %d\n", rules.num_players);
    printf("Num colors: %d\n", rules.num_colors);
    printf("Num max guesses: %d\n", rules.max_guesses);
    printf("Num rounds: %d\n", rules.num_rounds);

exit:
    printf("Game ended, disconnected from server\n");
    free(nickname);
    close(sock);
}
