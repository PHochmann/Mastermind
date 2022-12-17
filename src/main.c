#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>

#include "console.h"
#include "mastermind.h"
#include "multiplayer/client.h"
#include "multiplayer/server.h"
#include "string_util.h"
#include "table.h"

#define RED "\033[38:2:255:000:000m"
#define GRN "\033[38:2:000:255:000m"
#define YEL "\033[38:2:250:237:000m"
#define BLU "\033[38:2:000:000:255m"
#define CYN "\033[38:2:065:253:254m"
#define ORN "\033[38:2:255:165:000m"
#define PIN "\033[38:2:219:112:147m"
#define DRG "\033[38:2:085:107:047m"
#define RST "\033[0m"

#define PORT 25567

static MM_Match *play_game(MM_Context *ctx, Code_t solution)
{
    Feedback_t feedback = 0;
    MM_Match *match     = mm_new_match(ctx, false);
    while (mm_get_state(match) == MM_MATCH_PENDING)
    {
        Code_t input = read_colors(ctx, mm_get_turns(match));
        feedback     = mm_get_feedback(ctx, input, solution);
        mm_constrain(match, input, feedback);
        print_feedback(ctx, feedback);
        printf("\n");
    }
    if (mm_get_state(match) == MM_MATCH_WON)
    {
        print_winning_message(mm_get_turns(match));
    }
    else
    {
        print_losing_message(mm_get_max_guesses(ctx));
        printf("Solution: ");
        print_colors(ctx, solution);
        printf("\n\n");
    }
    return match;
}

static void multiplayer(MM_Context *ctx, const char *const *colors)
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
                char *ip = readline("Server IP Address: ");
                clear_input();
                play_client(ip, PORT, colors);
                free(ip);
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

static void recommender(MM_Context *ctx, MM_Strategy strategy)
{
    MM_Match *match     = mm_new_match(ctx, true);
    Feedback_t feedback = 0;
    while (!mm_is_winning_feedback(ctx, feedback))
    {
        uint16_t recommendation = mm_recommend(match, strategy);
        print_colors(ctx, recommendation);
        printf("\n");
        feedback = read_feedback(ctx);
        print_feedback(ctx, feedback);
        printf("\n");
        mm_constrain(match, recommendation, feedback);
        if (mm_get_remaining_solutions(match) == 0)
        {
            printf("That's not possible - inconsistent feedback given. Game "
                   "aborted.\n\n");
            mm_free_match(match);
            return;
        }
    }
    print_winning_message(mm_get_turns(match));
    mm_free_match(match);
}

static void options(MM_Context **ctx, const char *const *colors)
{
    int max_guesses, num_slots, num_colors;
    if (readline_int("Max guesses", mm_get_max_guesses(*ctx), 1, MAX_MAX_GUESSES, &max_guesses)
        && readline_int("Number of slots", mm_get_num_slots(*ctx), 1, MAX_NUM_SLOTS, &num_slots)
        && readline_int("Number of colors", mm_get_num_colors(*ctx), 1, MAX_NUM_COLORS, &num_colors))
    {
        mm_free_ctx(*ctx);
        *ctx = mm_new_ctx(max_guesses, num_slots, num_colors, colors);
    }
}

static void singleplayer(MM_Context *ctx)
{
    mm_free_match(play_game(ctx, rand() % mm_get_num_codes(ctx)));
}

int main()
{
    printf("Mastermind v1.0.0 (c) 2022 Philipp Hochmann\n");
    srand(time(NULL));

    const char *const colors[] = { ORN "Orange" RST, RED " Red  " RST,
                                   YEL "Yellow" RST, BLU " Blue " RST,
                                   CYN " Cyan " RST, GRN "Green " RST,
                                   DRG "DGreen" RST, PIN "Pink " RST };

    MM_Context *ctx = mm_new_ctx(10, 4, 6, colors);

    while (true)
    {
        char *input = readline("(s)ingleplayer, (m)ultiplayer, (r)ecommender, (o)ptions or (e)xit? ");
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
            case 'r':
                recommender(ctx, MM_STRAT_AVERAGE);
                break;
            case 'm':
                multiplayer(ctx, colors);
                break;
            case 'o':
                options(&ctx, colors);
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
