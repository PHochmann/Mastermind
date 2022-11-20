#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mastermind.h"
#include "mm_decoder.h"

#define MAX(a, b) (a >= b ? a : b)

struct MM_Decoder
{
    bool solspace[MM_NUM_INPUTS];
    uint16_t num_solutions;
    uint8_t num_turns;
    uint8_t last_feedback;
};

static uint16_t recomm_1_lookup[MM_NUM_STRATEGIES];
static bool recomm_1_init[MM_NUM_STRATEGIES];
static uint16_t recomm_2_lookup[MM_NUM_STRATEGIES][MM_NUM_FEEDBACKS];
static bool recomm_2_init[MM_NUM_STRATEGIES];

/*
 * PRIVATE FUNCTIONS
 */



/*
 * PUBLIC FUNCTIONS
 */

void mm_init_recommendation_lookup()
{
    if (recomm_1_init[0] && recomm_2_init[0])
    {
        return;
    }

    printf("Initializing data structures...\n");

    /* We build a lookup table for the first and second recommendation.
     * This drastically improves performance since computation time for mm_recommend()
     * decreases a lot for further guesses because of the smaller solution space.
     */
    for (uint8_t strat = 0; strat < MM_NUM_STRATEGIES; strat++)
    {
        // 1. recommendation
        MM_Decoder *mm = mm_new_decoder();
        recomm_1_lookup[strat] = mm_recommend(mm, strat);
        recomm_1_init[strat] = true;
        mm_end_decoder(mm);

        // 2. recommendation
        for (uint8_t fb = 0; fb < MM_NUM_FEEDBACKS; fb++)
        {
            MM_Decoder *mm = mm_new_decoder();
            mm_constrain(mm, mm_recommend(mm, strat), fb);
            recomm_2_lookup[strat][fb] = mm_recommend(mm, strat);
            mm_end_decoder(mm);
        }
        recomm_2_init[strat] = true;
    }
}

MM_Decoder *mm_new_decoder()
{
    mm_init();
    MM_Decoder *result = malloc(sizeof(MM_Decoder));
    for (uint16_t i = 0; i < MM_NUM_INPUTS; i++)
    {
        result->solspace[i] = true;
    }
    result->num_solutions = MM_NUM_INPUTS;
    result->num_turns     = 0;
    return result;
}

void mm_end_decoder(MM_Decoder* mm)
{
    free(mm);
}

uint16_t mm_recommend(MM_Decoder *mm, Strategy strat)
{
    if (recomm_1_init[strat] && mm->num_turns == 0)
    {
        return recomm_1_lookup[strat];
    }
    else if (recomm_2_init[strat] && mm->num_turns == 1)
    {
        return recomm_2_lookup[strat][mm->last_feedback];
    }

    uint32_t aggregations[MM_NUM_INPUTS];
    for (uint16_t i = 0; i < MM_NUM_INPUTS; i++)
    {
        aggregations[i] = UINT32_MAX;
    }

    for (uint16_t i = 0; i < MM_NUM_INPUTS; i++)
    {
        if (mm->solspace[i])
        {
            aggregations[i] = 0;
            for (uint16_t j = 0; j < MM_NUM_INPUTS; j++)
            {
                uint8_t fb = mm_get_feedback(j, i);
                uint16_t num_solutions = 0;
                for (uint16_t k = 0; k < MM_NUM_INPUTS; k++)
                {
                    if (mm->solspace[k])
                    {
                        if (mm_get_feedback(j, k) == fb)
                        {
                            num_solutions++;
                        }
                    }
                }

                switch (strat)
                {
                    case MM_STRAT_AVERAGE:
                        aggregations[i] += num_solutions;
                        break;
                    case MM_STRAT_MINMAX:
                        aggregations[i] = MAX(aggregations[i], num_solutions);
                        break;
                    default:
                        break;
                }

            }
        }
    }

    uint16_t min = 0;
    for (uint16_t i = 0; i < MM_NUM_INPUTS; i++)
    {
        if (aggregations[i] < aggregations[min])
        {
            min = i;
        }
    }

    return min;
}

void mm_constrain(MM_Decoder* mm, uint16_t input, uint8_t feedback)
{
    uint16_t remaining = 0;
    for (uint16_t i = 0; i < MM_NUM_INPUTS; i++)
    {
        if (mm->solspace[i])
        {
            if (mm_get_feedback(input, i) != feedback)
            {
                mm->solspace[i] = false;
            }
            else
            {
                remaining++;
            }
        }
    }
    mm->num_solutions = remaining;
    mm->num_turns++;
    mm->last_feedback = feedback;
}

// Getters

uint16_t mm_get_remaining_solutions(MM_Decoder *mm)
{
    return mm->num_solutions;
}

uint16_t mm_get_turns(MM_Decoder *mm)
{
    return mm->num_turns;
}
