#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#include "mastermind.h"
#include "console.h"
#include "table.h"

#define MAX(a, b) (a >= b ? a : b)

#define PLAYER_A "Justine"
#define PLAYER_B "Philipp"

MM_Match *play_game(MM_Context *ctx, uint16_t solution)
{
    uint8_t feedback = 0;
    MM_Match *match = mm_new_match(ctx, false);
    while (!mm_is_winning_feedback(ctx, feedback) && mm_get_turns(match) < mm_get_max_guesses(ctx))
    {
        uint16_t input = read_colors(ctx, mm_get_turns(match));
        feedback = mm_get_feedback(ctx, input, solution);
        mm_constrain(match, input, feedback);
        print_feedback(ctx, feedback);
        printf("\n");
    }
    if (mm_get_turns(match) == mm_get_max_guesses(ctx) + 1)
    {
        print_losing_message(mm_get_max_guesses(ctx));
        printf("Solution: ");
        print_colors(ctx, solution);
        printf("\n\n");
    }
    else
    {
        print_winning_message(mm_get_turns(match));
    }
    return match;
}

// Public

void play_recommender(MM_Context *ctx, MM_Strategy strategy)
{
    MM_Match *match = mm_new_match(ctx, true);
    uint8_t feedback = 0;
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
            printf("That's not possible - inconsistent feedback given. Game aborted.\n\n");
            mm_free_match(match);
            return;
        }
    }
    print_winning_message(mm_get_turns(match));
    mm_free_match(match);
}

void play_singleplayer(MM_Context *ctx)
{
    mm_free_match(play_game(ctx, rand() % mm_get_num_codes(ctx)));
}

void play_duel(MM_Context *ctx, uint8_t num_rounds)
{
    uint16_t points_a = 0;
    uint16_t points_b = 0;
    for (uint8_t i = 0; i < num_rounds; i++)
    {
        clear_screen();
        uint16_t solution = rand() % mm_get_num_codes(ctx);

        printf("Round %d - %s's turn\n", i + 1, PLAYER_A);
        MM_Match *match_a = play_game(ctx, solution);
        any_key();
        clear_screen();
        printf("Round %d - %s's turn\n", i + 1, PLAYER_B);
        MM_Match *match_b = play_game(ctx, solution);
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

        for (uint8_t j = 0; j < MAX(mm_get_turns(match_a), mm_get_turns(match_b)) + 1; j++)
        {
            if (j < mm_get_turns(match_a))
            {
                add_cell_gc(tbl, get_colors_string(ctx, mm_get_history_guess(match_a, j)));
                uint8_t b, w;
                mm_code_to_feedback(ctx, mm_get_history_feedback(match_a, j), &b, &w);
                add_cell_fmt(tbl, " %d ", b);
                add_cell_fmt(tbl, " %d ", w);
            } else
            {
                add_empty_cell(tbl);
                add_empty_cell(tbl);
                add_empty_cell(tbl);
            }

            if (j < mm_get_turns(match_b))
            {
                add_cell_gc(tbl, get_colors_string(ctx, mm_get_history_guess(match_b, j)));
                uint8_t b, w;
                mm_code_to_feedback(ctx, mm_get_history_feedback(match_b, j), &b, &w);
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
        add_cell_fmt(tbl, " %d turns", mm_get_turns(match_a));
        set_span(tbl, 3, 1);
        add_cell_fmt(tbl, " %d turns", mm_get_turns(match_b));
        next_row(tbl);
        make_boxed(tbl, BORDER_SINGLE);
        print_table(tbl);
        free_table(tbl);
        printf("\n");
        any_key();

        if (mm_get_turns(match_a) < mm_get_turns(match_b))
        {
            points_a++;
        } else if (mm_get_turns(match_a) > mm_get_turns(match_b))
        {
            points_b++;
        }

        mm_free_match(match_a);
        mm_free_match(match_b);
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
