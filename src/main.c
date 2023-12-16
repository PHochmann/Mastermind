#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>

#include "util/string_util.h"
#include "util/table.h"
#include "util/console.h"
#include "mastermind.h"
#include "multiplayer/client.h"
#include "multiplayer/server.h"
#include "quickie.h"

#define DEFAULT_IP "127.0.0.1"
#define PORT       25567

#define DEFAULT_MAX_GUESSES 10
#define DEFAULT_NUM_COLORS  6
#define DEFAULT_NUM_SLOTS   4

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
        print_guess(mm_get_turns(match) - 1, match, true);
        printf("\n");
    }
    print_match_end_message(match, solution, true);
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
    if (readline_int("Max guesses", DEFAULT_MAX_GUESSES, 2, MM_MAX_MAX_GUESSES, &max_guesses)
        && readline_int("Number of slots", DEFAULT_NUM_SLOTS, 2, MM_MAX_NUM_SLOTS, &num_slots)
        && readline_int("Number of colors", DEFAULT_NUM_COLORS, 2, MM_MAX_NUM_COLORS, &num_colors))
    {
        mm_free_ctx(*ctx);
        *ctx = mm_new_ctx(max_guesses, num_slots, num_colors);
        set_pending_fb_scores();
    }
}

static void singleplayer(MM_Context *ctx)
{
    mm_free_match(play_game(ctx, rand() % mm_get_num_codes(ctx)));
}

static void credits()
{
    Table *tbl = tbl_get_new();
    tbl_set_span(tbl, 2, 1);
    tbl_override_horizontal_alignment(tbl, TBL_H_ALIGN_CENTER);
    tbl_add_cell(tbl, " Credits ");
    tbl_next_row(tbl);
    tbl_set_span(tbl, 2, 1);
    tbl_set_hline(tbl, TBL_BORDER_SINGLE);
    tbl_override_horizontal_alignment(tbl, TBL_H_ALIGN_CENTER);
    tbl_add_cell(tbl, " Mastermind (c) 2023\n https://github.com/PHochmann/Mastermind.git ");
    tbl_next_row(tbl);
    tbl_add_cell(tbl, " Code: ");
    tbl_add_cell(tbl, "Philipp Hochmann ");
    tbl_next_row(tbl);
    tbl_add_cell(tbl, " Algorithms: ");
    tbl_add_cell(tbl, "Justine B. Geuenich ");
    tbl_next_row(tbl);
    tbl_make_boxed(tbl, TBL_BORDER_SINGLE);
    tbl_set_alternative_style(tbl);
    tbl_print(tbl);
    tbl_free(tbl);
}

int main()
{
    srand(time(NULL));
    MM_Context *ctx = mm_new_ctx(DEFAULT_MAX_GUESSES, DEFAULT_NUM_SLOTS, DEFAULT_NUM_COLORS);
    printf("~ ~ Mastermind ~ ~\n");

    while (true)
    {
        char *input = readline("(s)ingleplayer, (m)ultiplayer, (f)ast, (o)ptions or (e)xit? ");
        clear_input();
        bool exit = false;

        if (input == NULL)
        {
            exit = true;
        }
        else
        {
            if (strlen(input) == 1)
            {
                switch (to_lower(input[0]))
                {
                case 's':
                    singleplayer(ctx);
                    break;
                case 'm':
                    multiplayer(ctx);
                    break;
                case 'f':
                    quickie(ctx);
                    break;
                case 'o':
                    options(&ctx);
                    break;
                case 'c':
                    credits();
                    break;
                case 'e':
                    exit = true;
                    break;
                }
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
