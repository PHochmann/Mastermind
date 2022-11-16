#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mastermind.h"

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a >= b ? a : b)

#define NUM_SLOTS        4
#define NUM_FEEDBACKS   14
#define NUM_INPUTS    1296

#define RED "\033[0;31m"
#define GRN "\033[0;32m"
#define YEL "\033[0;33m"
#define BLU "\033[0;34m"
#define CYN "\033[0;36m"
#define ORN "\033[38:2:255:165:0m"
#define RES "\033[0m"

typedef enum
{
    COL_ORANGE = 0,
    COL_RED,
    COL_YELLOW,
    COL_BLUE,
    COL_GREEN,
    COL_CYAN,
    NUM_COLORS
} Color;

struct MasterMind
{
    bool solspace[NUM_INPUTS];
    uint16_t num_solutions;
    uint8_t num_turns;
};

static bool initialized = false;
static uint8_t feedback_encode[NUM_SLOTS + 1][NUM_SLOTS + 1]; // First index: num_b, second index: num_w
//static uint16_t feedback_decode[NUM_FEEDBACKS];
static uint8_t feedback_lookup[NUM_INPUTS][NUM_INPUTS];

/*
 * PRIVATE FUNCTIONS
 */

static char *col_to_str(Color col)
{
    switch (col)
    {
        case COL_ORANGE:
            return ORN "Orange" RES;
        case COL_RED:
            return RED "Red" RES;
        case COL_YELLOW:
            return YEL "Yellow" RES;
        case COL_BLUE:
            return BLU "Blue" RES;
        case COL_CYAN:
            return CYN "Cyan" RES;
        case COL_GREEN:
            return GRN "Green" RES;
        default:
            return "Error";
    }
}

static Color get_color(uint16_t code, uint8_t index)
{
    return (int)(code / pow(NUM_COLORS, index)) % NUM_COLORS;
}

/*static void code_to_feedback(uint8_t code, uint8_t *b, uint8_t *w)
{
    *b = feedback_decode[code] >> 8;
    *w = feedback_decode[code] & 0xFF;
}*/

static uint16_t color_to_code(Color a, Color b, Color c, Color d)
{
    return a + NUM_COLORS * b + pow(NUM_COLORS, 2) * c + pow(NUM_COLORS, 3) * d;
}

static void init_feedback()
{
    uint8_t counter = 0;
    for (uint8_t b = 0; b <= NUM_SLOTS; b++)
    {
        for (uint8_t w = 0; w <= NUM_SLOTS; w++)
        {
            if (b + w <= NUM_SLOTS && !(b == NUM_SLOTS - 1 && w == 1))
            {
                feedback_encode[b][w] = counter;
                //feedback_decode[counter] = (b << 8) | w;
                counter++;
            }
        }
    }

    for (uint16_t input = 0; input < NUM_INPUTS; input++)
    {
        for (uint16_t solution = 0; solution < NUM_INPUTS; solution++)
        {
            uint8_t num_b = 0;
            uint8_t num_w = 0;
            uint8_t color_counts_a[NUM_COLORS] = { 0 };
            uint8_t color_counts_b[NUM_COLORS] = { 0 };

            for (uint8_t i = 0; i < NUM_SLOTS; i++)
            {
                Color a = get_color(input, i);
                Color b = get_color(solution, i);
                color_counts_a[a]++;
                color_counts_b[b]++;
                if (a == b)
                {
                    num_b++;
                }
            }

            for (uint8_t i = 0; i < NUM_COLORS; i++)
            {
                num_w += MIN(color_counts_a[i], color_counts_b[i]);
            }
            num_w -= num_b;

            feedback_lookup[input][solution] = feedback_encode[num_b][num_w];
        }
    }
}

/*
 * PUBLIC FUNCTIONS
 */

void mm_init()
{
    init_feedback();
    initialized = true;
}

MasterMind *mm_get_new_game()
{
    if (!initialized)
    {
        mm_init();
    }

    MasterMind *result = malloc(sizeof(MasterMind));
    for (uint16_t i = 0; i < NUM_INPUTS; i++)
    {
        result->solspace[i] = true;
    }
    result->num_solutions = NUM_INPUTS;
    result->num_turns     = 0;
    return result;
}

void mm_end_game(MasterMind* mm)
{
    free(mm);
}

uint16_t mm_recommend(MasterMind *mm, Strategy strat)
{
    if (mm->num_solutions == NUM_INPUTS)
    {
        return color_to_code(COL_BLUE, COL_BLUE, COL_RED, COL_RED);
    }

    uint32_t aggregations[NUM_INPUTS];
    for (uint16_t i = 0; i < NUM_INPUTS; i++)
    {
        aggregations[i] = UINT32_MAX;
    }

    for (uint16_t i = 0; i < NUM_INPUTS; i++)
    {
        if (mm->solspace[i])
        {
            aggregations[i] = 0;
            for (uint16_t j = 0; j < NUM_INPUTS; j++)
            {
                uint8_t fb = feedback_lookup[j][i];
                uint16_t num_solutions = 0;
                for (uint16_t k = 0; k < NUM_INPUTS; k++)
                {
                    if (mm->solspace[k])
                    {
                        if (feedback_lookup[j][k] == fb)
                        {
                            num_solutions++;
                        }
                    }
                }

                switch (strat)
                {
                    case STRAT_AVERAGE:
                        aggregations[i] += num_solutions;
                        break;
                    case STRAT_MINMAX:
                        aggregations[i] = MAX(aggregations[i], num_solutions);
                        break;
                }

            }
        }
    }

    uint16_t min = 0;
    for (uint16_t i = 0; i < NUM_INPUTS; i++)
    {
        if (aggregations[i] < aggregations[min])
        {
            min = i;
        }
    }

    return min;
}

void mm_constrain(MasterMind* mm, uint16_t input, uint8_t feedback)
{
    uint16_t remaining = 0;
    for (uint16_t i = 0; i < NUM_INPUTS; i++)
    {
        if (mm->solspace[i])
        {
            if (feedback_lookup[input][i] != feedback)
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
}

double mm_measure_average(Strategy strat)
{
    uint32_t turns = 0;
    for (uint16_t i = 0; i < NUM_INPUTS; i++)
    {
        MasterMind *mm = mm_get_new_game();
        uint8_t fb = 0;
        while (!mm_is_winning_feedback(fb))
        {
            uint16_t recommendation = mm_recommend(mm, strat);
            fb = feedback_lookup[recommendation][i];
            mm_constrain(mm, recommendation, fb);
            turns++;
        }
        mm_end_game(mm);
        /*if (i % 10 == 0)
        {
            printf("%f\n", (float)i / NUM_INPUTS);
        }*/
    }
    return (double)turns / NUM_INPUTS;
}

bool mm_is_winning_feedback(uint8_t fb)
{
    return fb == feedback_encode[NUM_SLOTS][0];
}

// Getters

uint16_t mm_get_remaining_solutions(MasterMind *mm)
{
    return mm->num_solutions;
}

uint16_t mm_get_turns(MasterMind *mm)
{
    return mm->num_turns;
}

// IO Functions

uint8_t mm_read_feedback()
{
    printf("#blacks, #whites: ");
    fflush(stdout);
    char inp[3];
    read(STDIN_FILENO, &inp, 3);
    return feedback_encode[inp[0] - '0'][inp[1] - '0'];
}

void mm_print_colors(uint16_t input)
{
    for (uint8_t i = 0; i < NUM_SLOTS; i++)
    {
        printf("%s, ", col_to_str(get_color(input, i)));
    }
    printf("\n");
}
