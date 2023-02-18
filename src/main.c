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

#define DEFAULT_MAX_GUESSES 10

static Code_t adjust_solution(MM_Match *match, Code_t input, Code_t curr_solution, int min_score, int max_score)
{
    if (mm_get_remaining_solutions(match) == 1)
    {
        return curr_solution;
    }

    MM_Context *ctx = mm_get_context(match);

    int viable = 0;
    for (Code_t i = 0; i < mm_get_num_codes(ctx); i++)
    {
        int score = mm_get_feedback_score(ctx, mm_get_feedback(ctx, input, i));
        if (mm_is_in_solution(match, i) && (score <= max_score && score >= min_score))
        {
            viable++;
        }
    }

    if (viable != 0)
    {
        int sol = rand() % viable;
        for (Code_t i = 0; i < mm_get_num_codes(ctx); i++)
        {
            int score = mm_get_feedback_score(ctx, mm_get_feedback(ctx, input, i));
            if (mm_is_in_solution(match, i) && (score <= max_score && score >= min_score))
            {
                if (sol == 0)
                {
                    return i;
                }
                else
                {
                    sol--;
                }
            }
        }
    }
    else
    {
        int sol = rand() % (mm_get_remaining_solutions(match) - 1);
        for (Code_t i = 0; i < mm_get_num_codes(ctx); i++)
        {
            if (mm_is_in_solution(match, i) && (i != input))
            {
                if (sol == 0)
                {
                    return i;
                }
                else
                {
                    sol--;
                }
            }
        }
    }
    return 0;
}

static void quickie(MM_Context *ctx)
{
    const int num_difficulties = 5;
    const int n_fb             = mm_get_num_feedbacks(ctx) - 1;
    const int score_width      = n_fb / 3;

    int difficulty;
    if (!readline_int("Difficulty", num_difficulties / 2, 1, num_difficulties, &difficulty))
    {
        return;
    }

    int score_min = ((n_fb - score_width) / (num_difficulties - 1)) * (num_difficulties - difficulty);
    int score_max = score_min + score_width;

    mm_init_feedback_lookup(ctx);
    Code_t solution = rand() % mm_get_num_codes(ctx);
    MM_Match *match = mm_new_match(ctx, true);

    while (mm_get_remaining_solutions(match) > 1)
    {
        Code_t guess;
        if (mm_get_turns(match) == 0)
        {
            guess = rand() % mm_get_num_codes(ctx);
        }
        else
        {
            guess = mm_recommend_guess(match);
        }
        solution = adjust_solution(match, guess, solution, score_min, score_max);
        mm_constrain(match, guess, mm_get_feedback(ctx, guess, solution));
        print_guess(mm_get_turns(match) - 1, match, true);
        printf("\n");
    }

    Code_t input;
    if (read_colors(ctx, -1, &input))
    {
        mm_constrain(match, input, mm_get_feedback(ctx, input, solution));
        print_guess(mm_get_turns(match) - 1, match, true);
        printf("\n");
        print_match_end_message(match, solution, false);
    }
    else
    {
        printf("Aborted\n");
    }

    mm_free_match(match);
}

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
    if (readline_int("Max guesses", DEFAULT_MAX_GUESSES, 2, MAX_MAX_GUESSES, &max_guesses)
        && readline_int("Number of slots", MM_DEFAULT_NUM_SLOTS, 2, MAX_NUM_SLOTS, &num_slots)
        && readline_int("Number of colors", MM_DEFAULT_NUM_COLORS, 2, MAX_NUM_COLORS, &num_colors))
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
    MM_Context *ctx = mm_new_ctx(DEFAULT_MAX_GUESSES, MM_DEFAULT_NUM_SLOTS, MM_DEFAULT_NUM_COLORS);

    while (true)
    {
        char *input = readline("(s)ingleplayer, (m)ultiplayer, (q)uickie, (o)ptions or (e)xit? ");
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
                case 'q':
                    quickie(ctx);
                    break;
                case 'o':
                    options(&ctx);
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
