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

void pressany()
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
    printf("~ ~ Game over - You could not make it in %d turns ~ ~\n\n", MM_NUM_MAX_GUESSES);
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
        out_feedbacks[mm_get_turns(mm)] = feedback;
        out_guesses[mm_get_turns(mm)] = input;
        mm_constrain(mm, input, feedback);
        mm_print_feedback(feedback);
        printf("\n");
    }
    if (mm_get_turns(mm) == MM_NUM_MAX_GUESSES + 1)
    {
        print_losing_message();
    }
    else
    {
        print_winning_message(mm_get_turns(mm));
    }
    *out_num_turns = mm_get_turns(mm);
    mm_end_decoder(mm);
}

void duel(uint8_t num_rounds)
{
    uint16_t total_turns_a = 0;
    uint16_t total_turns_b = 0;
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
        pressany();
        clear_screen();
        printf("Round %d - %s's turn\n", i + 1, PLAYER_B);
        play_game(solution, guesses_b, feedbacks_b, &turns_b);
        pressany();
        clear_screen();

        printf("Summary of round %d:\n\n", i + 1);
        Table *tbl = get_empty_table();
        add_cell(tbl, " " PLAYER_A " ");
        set_vline(tbl, 1, BORDER_SINGLE);
        add_cell(tbl, " " PLAYER_B " ");
        next_row(tbl);
        set_hline(tbl, BORDER_SINGLE);
        for (uint8_t j = 0; j < MAX(turns_a, turns_b) + 1; j++)
        {
            if (j == turns_a)
            {
                add_cell_fmt(tbl, " ~ ~ %d turns ~ ~ ", turns_a);
            } else if (j < turns_a)
            {
                add_cell_gc(tbl, mm_get_colors_string(guesses_a[j]));
            } else if (j > turns_a)
            {
                add_empty_cell(tbl);
            }

            if (j == turns_b)
            {
                add_cell_fmt(tbl, " ~ ~ %d turns ~ ~ ", turns_b);
            } else if (j < turns_b)
            {
                add_cell_gc(tbl, mm_get_colors_string(guesses_b[j]));
            } else if (j > turns_b)
            {
                add_empty_cell(tbl);
            }
            next_row(tbl);
        }
        print_table(tbl);
        free_table(tbl);
        printf("\n");
        pressany();

        total_turns_a = turns_a;
        total_turns_b = turns_b;
    }

    if (total_turns_a == total_turns_b)
    {
        printf("Draw!\n");
    }
    else
    {
        const char *winner = total_turns_a < total_turns_b ? PLAYER_A : PLAYER_B;
        printf("%s wins! Congratulations! %s took %d turns, %s took %d turns\n", winner, PLAYER_A, total_turns_a, PLAYER_B, total_turns_b);
    }
}

int main()
{
    srand(time(NULL));
    mm_init();
    while (true)
    {
        printf("(d)uel or (r)ecommender? ");
        fflush(stdout);
        char inp[2];
        read(STDIN_FILENO, &inp, 2);
        clear_input();
        switch(inp[0])
        {
            case 'd':
                duel(1);
                break;
            case 'r':
                play_decoder();
                break;
        }
    }
    return 0;
}
