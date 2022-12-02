#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <readline/readline.h>

#include "mastermind.h"
#include "table.h"
#include "console.h"

#define MAX_MAX_GUESSES 20

#define RED "\033[38:2:255:000:000m"
#define GRN "\033[38:2:000:255:000m"
#define YEL "\033[38:2:250:237:000m"
#define BLU "\033[38:2:000:000:255m"
#define CYN "\033[38:2:065:253:254m"
#define ORN "\033[38:2:255:165:000m"
#define PIN "\033[38:2:219:112:147m"
#define DRG "\033[38:2:085:107:047m"
#define RST "\033[0m"

#define PLAYER_A "Justine"
#define PLAYER_B "Philipp"

#define MAX(a, b) (a >= b ? a : b)

const MM_Strategy strategy = MM_STRAT_AVERAGE;

void print_winning_message(uint8_t num_turns)
{
    printf("~ ~ Game won in %d steps ~ ~\n\n", num_turns);
}

void print_losing_message(uint8_t num_max_guesses)
{
    printf("~ ~ Game over - You could not make it in %d turns ~ ~\n", num_max_guesses);
}

void play_game(MM_Context *ctx, uint16_t solution, uint16_t *out_guesses, uint8_t *out_feedbacks, uint16_t *out_num_turns)
{
    uint8_t feedback = 0;
    MM_Match *match = mm_new_match(ctx, false);
    while (!mm_is_winning_feedback(ctx, feedback) && mm_get_turns(match) < mm_get_max_guesses(ctx))
    {
        uint16_t input = mm_read_colors(ctx, mm_get_turns(match));
        mm_print_colors(ctx, input);
        printf("\n");
        feedback = mm_get_feedback(ctx, input, solution);
        if (out_feedbacks != NULL)
        {
            out_feedbacks[mm_get_turns(match)] = feedback;
        }
        if (out_guesses != NULL)
        {
            out_guesses[mm_get_turns(match)] = input;
        }
        mm_constrain(match, input, feedback);
        mm_print_feedback(ctx, feedback);
        printf("\n");
    }
    if (mm_get_turns(match) == mm_get_max_guesses(ctx) + 1)
    {
        print_losing_message(mm_get_max_guesses(ctx));
        printf("Solution: ");
        mm_print_colors(ctx, solution);
        printf("\n\n");
    }
    else
    {
        print_winning_message(mm_get_turns(match));
    }
    if (out_num_turns != NULL)
    {
        *out_num_turns = mm_get_turns(match);
    }
    mm_free_match(match);
}

void recommender(MM_Context *ctx)
{
    MM_Match *match = mm_new_match(ctx, true);
    uint8_t feedback = 0;
    while (!mm_is_winning_feedback(ctx, feedback))
    {
        uint16_t recommendation = mm_recommend(match, strategy);
        mm_print_colors(ctx, recommendation);
        printf("\n");
        feedback = mm_read_feedback(ctx);
        mm_print_feedback(ctx, feedback);
        printf("\n");
        mm_constrain(match, recommendation, feedback);
        if (mm_get_remaining_solutions(match) == 0)
        {
            printf("That's not possible - inconsistent feedback given. Game aborted.\n\n");
            mm_free_match(match);
            return;
        }
    }
    print_winning_message(mm_get_turns(match));
    mm_free_match(match);
}

void singleplayer(MM_Context *ctx)
{
    uint16_t solution = rand() % mm_get_num_inputs(ctx);
    play_game(ctx, solution, NULL, NULL, NULL);
}

void duel(MM_Context *ctx, uint8_t num_rounds)
{
    uint16_t points_a = 0;
    uint16_t points_b = 0;
    for (uint8_t i = 0; i < num_rounds; i++)
    {
        clear_screen();
        uint16_t solution = rand() % mm_get_num_inputs(ctx);
        uint16_t turns_a = 0;
        uint16_t turns_b = 0;
        uint16_t guesses_a[MAX_MAX_GUESSES];
        uint16_t guesses_b[MAX_MAX_GUESSES];
        uint8_t feedbacks_a[MAX_MAX_GUESSES];
        uint8_t feedbacks_b[MAX_MAX_GUESSES];

        printf("Round %d - %s's turn\n", i + 1, PLAYER_A);
        play_game(ctx, solution, guesses_a, feedbacks_a, &turns_a);
        any_key();
        clear_screen();
        printf("Round %d - %s's turn\n", i + 1, PLAYER_B);
        play_game(ctx, solution, guesses_b, feedbacks_b, &turns_b);
        any_key();
        clear_screen();

        Table *tbl = get_empty_table();
        set_span(tbl, 6, 1);
        override_horizontal_alignment(tbl, H_ALIGN_CENTER);
        add_cell_fmt(tbl, "Summary of round 1", i + 1);
        next_row(tbl);
        set_hline(tbl, BORDER_SINGLE);
        add_cell(tbl, " " PLAYER_A " ");
        add_cell(tbl, "#B");
        add_cell(tbl, "#W ");
        add_cell(tbl, " " PLAYER_B " ");
        add_cell(tbl, "#B");
        add_cell(tbl, "#W ");
        next_row(tbl);
        set_vline(tbl, 3, BORDER_SINGLE);
        set_hline(tbl, BORDER_SINGLE);

        for (uint8_t j = 0; j < MAX(turns_a, turns_b) + 1; j++)
        {
            if (j < turns_a)
            {
                add_cell_gc(tbl, mm_get_colors_string(ctx, guesses_a[j]));
                uint8_t b, w;
                mm_code_to_feedback(ctx, feedbacks_a[j], &b, &w);
                add_cell_fmt(tbl, " %d ", b);
                add_cell_fmt(tbl, " %d ", w);
            } else
            {
                add_empty_cell(tbl);
                add_empty_cell(tbl);
                add_empty_cell(tbl);
            }

            if (j < turns_b)
            {
                add_cell_gc(tbl, mm_get_colors_string(ctx, guesses_b[j]));
                uint8_t b, w;
                mm_code_to_feedback(ctx, feedbacks_b[j], &b, &w);
                add_cell_fmt(tbl, " %d ", b);
                add_cell_fmt(tbl, " %d ", w);
            } else
            {
                add_empty_cell(tbl);
                add_empty_cell(tbl);
                add_empty_cell(tbl);
            }
            next_row(tbl);
        }
        set_hline(tbl, BORDER_SINGLE);
        set_span(tbl, 3, 1);
        add_cell_fmt(tbl, " %d turns", turns_a);
        set_span(tbl, 3, 1);
        add_cell_fmt(tbl, " %d turns", turns_b);
        next_row(tbl);
        make_boxed(tbl, BORDER_SINGLE);
        print_table(tbl);
        free_table(tbl);
        printf("\n");
        any_key();

        if (turns_a < turns_b)
        {
            points_a++;
        } else if (turns_b < turns_a)
        {
            points_b++;
        }
    }

    if (points_a == points_b)
    {
        printf("Draw!\n");
    }
    else
    {
        const char *winner = points_a > points_b ? PLAYER_A : PLAYER_B;
        printf("%s wins! Congratulations! %s: %d points, %s: %d points\n", winner, PLAYER_A, points_a, PLAYER_B, points_b);
    }
}

int main()
{
    const char * const colors[] = {
        ORN "Orange" RST,
        RED " Red  " RST,
        YEL "Yellow" RST,
        BLU " Blue " RST,
        CYN " Cyan " RST,
        GRN "Green " RST,
        DRG "DGreen" RST,
        PIN "Pink " RST
    };

    MM_Context *very_easy_ctx = mm_new_ctx(4, 3, 3, colors);
    MM_Context *easy_ctx = mm_new_ctx(10, 4, 6, colors);
    MM_Context *hard_ctx = mm_new_ctx(12, 5, 8, colors);
    srand(time(NULL));
    while (true)
    {
        char *input = readline("(s)ingle player, (d)uel, (r)ecommender or (e)xit? ");
        clear_input();
        bool exit = false;

        switch (input[0])
        {
            case 's':
                singleplayer(very_easy_ctx);
                break;
            case 'd':
                duel(easy_ctx, 4);
                break;
            case 'r':
                recommender(very_easy_ctx);
                break;
            case 'e':
                exit = true;
                break;
        }
        free(input);
        if (exit)
        {
            break;
        }
    }

    mm_free_ctx(very_easy_ctx);
    mm_free_ctx(easy_ctx);
    mm_free_ctx(hard_ctx);
    return 0;
}
