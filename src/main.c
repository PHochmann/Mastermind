#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>

#include "util/console.h"
#include "mastermind.h"
#include "multiplayer/client.h"
#include "multiplayer/server.h"
#include "util/string_util.h"
#include "util/table.h"

#define DEFAULT_IP "127.0.0.1"
#define PORT       25567

static MM_Match *play_game(MM_Context *ctx, Code_t solution)
{
    Feedback_t feedback = 0;
    MM_Match *match     = mm_new_match(ctx, true);
    while (mm_get_state(match) == MM_MATCH_PENDING)
    {
        Code_t input;
        if (!read_colors(ctx, mm_get_turns(match) + 1, &input))
        {
            printf("Aborted. Solution was: ");
            print_colors(ctx, solution);
            printf("\n");
            return match;
        }
        feedback = mm_get_feedback(ctx, input, solution);
        mm_constrain(match, input, feedback);
        print_guess(mm_get_turns(match) - 1, match);
        printf("\n");
    }
    if (mm_get_state(match) == MM_MATCH_WON)
    {
        print_winning_message(mm_get_turns(match));
    }
    else
    {
        print_losing_message(ctx, solution);
    }
    return match;
}

static void multiplayer(MM_Context *ctx)
{
    char *input = NULL;
    while (true)
    {
        input = readline("(c)lient, (s)erver or (b)ack? ");
        clear_input();
        bool exit = false;
        if (input == NULL)
        {
            exit = true;
        }
        else
        {
            switch (to_lower(input[0]))
            {
            case 'c':
            {
                char *ip = readline_fmt("Server IP Address (default %s): ", DEFAULT_IP);
                clear_input();
                if (ip == NULL)
                {
                    exit = true;
                }
                else
                {
                    if (ip[0] == '\0')
                    {
                        play_client(DEFAULT_IP, PORT);
                    }
                    else
                    {
                        play_client(ip, PORT);
                    }
                    free(ip);
                }
                break;
            }
            case 's':
            {
                int num_rounds, num_players;
                if (!readline_int("Number of rounds", 3, 1, MAX_NUM_ROUNDS, &num_rounds)
                    || !readline_int("Number of players", 2, 1, MAX_NUM_PLAYERS, &num_players))
                {
                    exit = true;
                }
                else
                {
                    start_server(ctx, num_players, num_rounds, PORT);
                }
                break;
            }
            case 'b':
                exit = true;
                break;
            }
            free(input);
        }
        if (exit)
        {
            return;
        }
    }
}

static void options(MM_Context **ctx)
{
    int max_guesses, num_slots, num_colors;
    if (readline_int("Max guesses", mm_get_max_guesses(*ctx), 2, MAX_MAX_GUESSES, &max_guesses)
        && readline_int("Number of slots", mm_get_num_slots(*ctx), 2, MAX_NUM_SLOTS, &num_slots)
        && readline_int("Number of colors", mm_get_num_colors(*ctx), 2, MAX_NUM_COLORS, &num_colors))
    {
        mm_free_ctx(*ctx);
        *ctx = mm_new_ctx(max_guesses, num_slots, num_colors);
    }
}

static void singleplayer(MM_Context *ctx)
{
    mm_free_match(play_game(ctx, rand() % mm_get_num_codes(ctx)));
}

int main()
{
    srand(time(NULL));
    MM_Context *ctx = mm_new_ctx(10, 4, 6);

    while (true)
    {
        char *input = readline("(s)ingleplayer, (m)ultiplayer, (o)ptions or (e)xit? ");
        clear_input();
        bool exit = false;

        if (input == NULL)
        {
            exit = true;
        }
        else
        {
            switch (to_lower(input[0]))
            {
            case 's':
                singleplayer(ctx);
                break;
            case 'm':
                multiplayer(ctx);
                break;
            case 'o':
                options(&ctx);
                break;
            case 'e':
                exit = true;
                break;
            }
            free(input);
        }
        if (exit)
        {
            break;
        }
    }

    mm_free_ctx(ctx);
    return 0;
}
