#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "mastermind.h"
#include "mm_decoder.h"
#include "table.h"

#define PLAYER_A "Justine"
#define PLAYER_B "Philipp"

#define MAX(a, b) (a >= b ? a : b)

const Strategy strategy = MM_STRAT_AVERAGE;

void clear_input()
{
    printf("\x1b[1F"); // Move to beginning of previous line
    printf("\x1b[2K"); // Clear entire line
}

void clear_screen()
{
    printf("\033[1;1H\033[2J");
}

void anykey()
{
    printf("Press enter to continue...\n");
    getchar();
}

void print_winning_message(uint8_t num_turns)
{
    printf("~ ~ Game won in %d steps ~ ~\n\n", num_turns);
}

void print_losing_message()
{
    printf("~ ~ Game over - You could not make it in %d turns ~ ~\n", MM_NUM_MAX_GUESSES);
}

void play_decoder()
{
    //mm_init_recommendation_lookup();
    MM_Decoder *mm = mm_new_decoder();
    uint8_t feedback = 0;
    while (!mm_is_winning_feedback(feedback))
    {
        uint16_t recommendation = mm_recommend(mm, strategy);
        mm_print_colors(recommendation);
        printf("\n");
        feedback = mm_read_feedback();
        clear_input();
        mm_print_feedback(feedback);
        printf("\n");
        mm_constrain(mm, recommendation, feedback);
        if (mm_get_remaining_solutions(mm) == 0)
        {
            printf("That's not possible - inconsistent feedback given. Game aborted.\n\n");
            mm_end_decoder(mm);
            return;
        }
    }
    print_winning_message(mm_get_turns(mm));
    mm_end_decoder(mm);
}

void play_game(uint16_t solution, uint16_t *out_guesses, uint8_t *out_feedbacks, uint16_t *out_num_turns)
{
    uint8_t feedback = 0;
    MM_Decoder *mm = mm_new_decoder();
    while (!mm_is_winning_feedback(feedback) && mm_get_turns(mm) <= MM_NUM_MAX_GUESSES)
    {
        uint16_t input = mm_read_colors();
        clear_input();
        mm_print_colors(input);
        printf("\n");
        feedback = mm_get_feedback(input, solution);
        if (out_feedbacks != NULL)
        {
            out_feedbacks[mm_get_turns(mm)] = feedback;
        }
        if (out_guesses != NULL)
        {
            out_guesses[mm_get_turns(mm)] = input;
        }
        mm_constrain(mm, input, feedback);
        mm_print_feedback(feedback);
        printf("\n");
    }
    if (mm_get_turns(mm) == MM_NUM_MAX_GUESSES + 1)
    {
        print_losing_message();
        printf("Solution: ");
        mm_print_colors(solution);
        printf("\n\n");
    }
    else
    {
        print_winning_message(mm_get_turns(mm));
    }
    if (out_num_turns != NULL)
    {
        *out_num_turns = mm_get_turns(mm);
    }
    mm_end_decoder(mm);
}

void singleplayer()
{
    uint16_t solution = rand() % MM_NUM_INPUTS;
    //mm_print_colors(solution);
    //printf("\n");
    play_game(solution, NULL, NULL, NULL);
}

void duel(uint8_t num_rounds)
{
    uint16_t points_a = 0;
    uint16_t points_b = 0;
    for (uint8_t i = 0; i < num_rounds; i++)
    {
        clear_screen();
        uint16_t solution = rand() % MM_NUM_INPUTS;
        uint16_t turns_a = 0;
        uint16_t turns_b = 0;
        uint16_t guesses_a[MM_NUM_MAX_GUESSES];
        uint16_t guesses_b[MM_NUM_MAX_GUESSES];
        uint8_t feedbacks_a[MM_NUM_MAX_GUESSES];
        uint8_t feedbacks_b[MM_NUM_MAX_GUESSES];

        printf("Round %d - %s's turn\n", i + 1, PLAYER_A);
        play_game(solution, guesses_a, feedbacks_a, &turns_a);
        anykey();
        clear_screen();
        printf("Round %d - %s's turn\n", i + 1, PLAYER_B);
        play_game(solution, guesses_b, feedbacks_b, &turns_b);
        anykey();
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
                add_cell_gc(tbl, mm_get_colors_string(guesses_a[j]));
                uint8_t b, w;
                mm_code_to_feedback(feedbacks_a[j], &b, &w);
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
                add_cell_gc(tbl, mm_get_colors_string(guesses_b[j]));
                uint8_t b, w;
                mm_code_to_feedback(feedbacks_b[j], &b, &w);
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
        anykey();

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
    srand(time(NULL));
    mm_init();
    while (true)
    {
        printf("(s)ingle player, (d)uel or (r)ecommender? ");
        fflush(stdout);
        char inp[2];
        read(STDIN_FILENO, &inp, 2);
        clear_input();
        switch(inp[0])
        {
            case 's':
                singleplayer();
                break;
            case 'd':
                duel(4);
                break;
            case 'r':
                play_decoder();
                break;
        }
    }
    return 0;
}
