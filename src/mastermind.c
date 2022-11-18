#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mastermind.h"

#define MIN(a, b) (a < b ? a : b)

#define RED "\033[0;31m"
#define GRN "\033[0;32m"
#define YEL "\033[38:2:250:237:39m"
#define BLU "\033[0;34m"
#define CYN "\033[0;36m"
#define ORN "\033[38:2:255:165:0m"
#define RES "\033[0m"

static bool initialized = false;
static uint8_t feedback_encode[MM_NUM_SLOTS + 1][MM_NUM_SLOTS + 1]; // First index: num_b, second index: num_w
static uint16_t feedback_decode[MM_NUM_FEEDBACKS];
static uint8_t feedback_lookup[MM_NUM_INPUTS][MM_NUM_INPUTS];

static char *col_to_str(Color col)
{
    switch (col)
    {
        case MM_COL_ORANGE:
            return ORN "Orange" RES;
        case MM_COL_RED:
            return RED "Red" RES;
        case MM_COL_YELLOW:
            return YEL "Yellow" RES;
        case MM_COL_BLUE:
            return BLU "Blue" RES;
        case MM_COL_CYAN:
            return CYN "Cyan" RES;
        case MM_COL_GREEN:
            return GRN "Green" RES;
        default:
            return "Error";
    }
}

static Color code_to_color(uint16_t code, uint8_t index)
{
    return (int)(code / pow(MM_NUM_COLORS, index)) % MM_NUM_COLORS;
}

static uint16_t colors_to_code(Color *colors)
{
    uint16_t result = 0;
    for (uint8_t i = 0; i < MM_NUM_SLOTS; i++)
    {
        result += colors[i] * pow(MM_NUM_COLORS, i);
    }
    return result;
}

static void code_to_feedback(uint8_t code, uint8_t *b, uint8_t *w)
{
    *b = feedback_decode[code] >> 8;
    *w = feedback_decode[code] & 0xFF;
}

void mm_init()
{
    uint8_t counter = 0;
    for (uint8_t b = 0; b <= MM_NUM_SLOTS; b++)
    {
        for (uint8_t w = 0; w <= MM_NUM_SLOTS; w++)
        {
            if (b + w <= MM_NUM_SLOTS && !(b == MM_NUM_SLOTS - 1 && w == 1))
            {
                feedback_encode[b][w] = counter;
                feedback_decode[counter] = (b << 8) | w;
                counter++;
            }
        }
    }

    for (uint16_t input = 0; input < MM_NUM_INPUTS; input++)
    {
        for (uint16_t solution = 0; solution < MM_NUM_INPUTS; solution++)
        {
            uint8_t num_b = 0;
            uint8_t num_w = 0;
            uint8_t color_counts_a[MM_NUM_COLORS] = { 0 };
            uint8_t color_counts_b[MM_NUM_COLORS] = { 0 };

            for (uint8_t i = 0; i < MM_NUM_SLOTS; i++)
            {
                Color a = code_to_color(input, i);
                Color b = code_to_color(solution, i);
                color_counts_a[a]++;
                color_counts_b[b]++;
                if (a == b)
                {
                    num_b++;
                }
            }

            for (uint8_t i = 0; i < MM_NUM_COLORS; i++)
            {
                num_w += MIN(color_counts_a[i], color_counts_b[i]);
            }
            num_w -= num_b;

            feedback_lookup[input][solution] = feedback_encode[num_b][num_w];
        }
    }
}

uint8_t mm_get_feedback(uint16_t input, uint16_t solution)
{
    if (!initialized)
    {
        mm_init();
    }
    return feedback_lookup[input][solution];
}

bool mm_is_winning_feedback(uint8_t fb)
{
    return fb == feedback_encode[MM_NUM_SLOTS][0];
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

uint16_t mm_read_input()
{
    printf("colors [orybcg]*%d: ", MM_NUM_SLOTS);
    fflush(stdout);
    char inp[MM_NUM_SLOTS + 1];
    Color colors[MM_NUM_SLOTS];
    read(STDIN_FILENO, &inp, MM_NUM_SLOTS + 1);
    for (uint8_t i = 0; i < MM_NUM_SLOTS; i++)
    {
        switch (inp[i])
        {
            case 'o':
                colors[i] = MM_COL_ORANGE;
                break;
            case 'r':
                colors[i] = MM_COL_RED;
                break;
            case 'y':
                colors[i] = MM_COL_YELLOW;
                break;
            case 'b':
                colors[i] = MM_COL_BLUE;
                break;
            case 'c':
                colors[i] = MM_COL_CYAN;
                break;
            case 'g':
                colors[i] = MM_COL_GREEN;
                break;
        }
    }
    return colors_to_code(colors);
}

void mm_print_colors(uint16_t input)
{
    for (uint8_t i = 0; i < MM_NUM_SLOTS; i++)
    {
        printf("%s  ", col_to_str(code_to_color(input, i)));
    }
    printf("\n");
}

void mm_print_feedback(uint8_t feedback)
{
    uint8_t b, w;
    code_to_feedback(feedback, &b, &w);
    printf("Black: %d\tWhite: %d\n", b, w);
}
