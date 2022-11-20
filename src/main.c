#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "mastermind.h"
#include "mm_decoder.h"

const Strategy strategy = MM_STRAT_AVERAGE;

double measure_average(Strategy strat)
{
    uint32_t turns = 0;
    for (uint16_t i = 0; i < MM_NUM_INPUTS; i++)
    {
        MM_Decoder *mm = mm_new_decoder();
        uint8_t fb = 0;
        while (!mm_is_winning_feedback(fb))
        {
            uint16_t recommendation = mm_recommend(mm, strat);
            fb = mm_get_feedback(recommendation, i);
            mm_constrain(mm, recommendation, fb);
            turns++;
        }
        mm_end_decoder(mm);
        if (i % 10 == 0)
        {
            printf("%f\n", (float)i / MM_NUM_INPUTS);
        }
    }
    return (double)turns / MM_NUM_INPUTS;
}

void compare_strategies()
{
    printf("Average number of turns for strategies:\n");
    printf("Minmax: %f\n", measure_average(MM_STRAT_MINMAX));
    printf("Average: %f\n", measure_average(MM_STRAT_AVERAGE));
}

void print_winning_message(uint8_t num_turns)
{
    printf("~ ~ Game won in %d steps ~ ~\n\n", num_turns);
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

void play_encoder()
{
    uint16_t solution = rand() % MM_NUM_INPUTS;
    uint8_t feedback = 0;
    MM_Decoder *mm = mm_new_decoder();
    while (!mm_is_winning_feedback(feedback))
    {
        uint16_t input = mm_read_input();
        feedback = mm_get_feedback(input, solution);
        mm_constrain(mm, input, feedback);
        mm_print_feedback(feedback);
        printf("\n");
    }
    print_winning_message(mm_get_turns(mm));
    mm_end_decoder(mm);
}

int main()
{
    srand(time(NULL));
    mm_init();
    while (true)
    {
        printf("Start (e)ncoder or (d)ecoder? ");
        fflush(stdout);
        char inp[2];
        read(STDIN_FILENO, &inp, 2);
        switch(inp[0])
        {
            case 'e':
                play_encoder();
                break;
            case 'd':
                play_decoder();
                break;
        }
    }
    return 0;
}
