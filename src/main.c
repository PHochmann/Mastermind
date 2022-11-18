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

void compare_averages()
{
    printf("Average number of turns for strategies:\n");
    printf("Minmax: %f\n", mm_measure_average(MM_STRAT_MINMAX));
    printf("Average: %f\n", mm_measure_average(MM_STRAT_AVERAGE));
}

void play_decoder()
{
    mm_init_recommendation_lookup();
    MM_Decoder *mm = mm_new_decoder();
    uint8_t feedback = 0;
    while (!mm_is_winning_feedback(feedback))
    {
        uint16_t recommendation = mm_recommend(mm, strategy);
        mm_print_colors(recommendation);
        feedback = mm_read_feedback();
        mm_constrain(mm, recommendation, feedback);
        if (mm_get_remaining_solutions(mm) == 0)
        {
            printf("That's not possible - inconsistent feedback given. Game aborted.\n\n");
            mm_end_decoder(mm);
            return;
        }
        else
        {
            if (mm_get_remaining_solutions(mm) != 1)
            {
                printf("%d solutions still possible\n", mm_get_remaining_solutions(mm));
            }
        }
    }
    printf("~ ~ Game won in %d steps ~ ~\n\n", mm_get_turns(mm));
    mm_end_decoder(mm);
}

void play_encoder()
{
    srand(time(NULL));
    uint16_t solution = rand() % MM_NUM_INPUTS;
    uint8_t feedback = 0;
    uint8_t counter = 0;
    while (!mm_is_winning_feedback(feedback))
    {
        uint16_t input = mm_read_input();
        printf("\033[F");
        mm_print_colors(input);
        feedback = mm_get_feedback(input, solution);
        mm_print_feedback(feedback);
        counter++;
    }
    printf("~ ~ Game won in %d steps ~ ~\n\n", counter);
}

int main()
{
    mm_init();
    while (true)
    {
        printf("(e)ncoder or (d)ecoder? ");
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
