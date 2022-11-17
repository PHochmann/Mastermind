#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include "mastermind.h"

const Strategy strategy = STRAT_AVERAGE;

void compare_averages()
{
    printf("Average number of turns for strategies:\n");
    printf("Minmax: %f\n", mm_measure_average(STRAT_MINMAX));
    printf("Average: %f\n", mm_measure_average(STRAT_AVERAGE));
}

void play()
{
    MasterMind *mm = mm_get_new_game();
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
            mm_end_game(mm);
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
    mm_end_game(mm);
}

int main()
{
    //compare_averages();
    while (true)
    {
        play();
    }
}
