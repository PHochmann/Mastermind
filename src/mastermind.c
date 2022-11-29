#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mastermind.h"
#include "util/string_builder.h"

#define MIN(a, b) (a < b ? a : b)

#define RED "\033[38:2:255:000:000m"
#define GRN "\033[38:2:000:255:000m"
#define YEL "\033[38:2:250:237:000m"
#define BLU "\033[38:2:000:000:255m"
#define CYN "\033[38:2:065:253:254m"
#define ORN "\033[38:2:255:165:000m"
#define PIN "\033[38:2:219:112:147m"
#define DRG "\033[38:2:085:107:047m"
#define BLK "\033[38:2:000:000:000m"
#define WHT "\033[38:2:255:255:255m"
#define BLK_BG "\033[48:2:000:000:000m"
#define WHT_BG "\033[48:2:255:255:255m"
#define RST "\033[0m"

static bool initialized    = false;
static bool fb_lookup_init = false;
static uint8_t feedback_encode[MM_NUM_SLOTS + 1][MM_NUM_SLOTS + 1]; // First index: num_b, second index: num_w
static uint16_t feedback_decode[MM_NUM_FEEDBACKS];
static uint8_t feedback_lookup[MM_NUM_INPUTS][MM_NUM_INPUTS];

typedef enum
{
    MM_COL_ORANGE,
    MM_COL_RED,
    MM_COL_YELLOW,
    MM_COL_BLUE,
    MM_COL_GREEN,
    MM_COL_CYAN,
    MM_COL_PINK,
    MM_COL_DARKGREEN
} Color;

static char *col_to_str(Color col)
{
    switch (col)
    {
        case MM_COL_ORANGE:
            return ORN "Orange" RST;
        case MM_COL_RED:
            return RED " Red  " RST;
        case MM_COL_YELLOW:
            return YEL "Yellow" RST;
        case MM_COL_BLUE:
            return BLU " Blue " RST;
        case MM_COL_CYAN:
            return CYN " Cyan " RST;
        case MM_COL_GREEN:
            return GRN "Green " RST;
        case MM_COL_PINK:
            return PIN " Pink " RST;
        case MM_COL_DARKGREEN:
            return DRG "DGreen" RST;
        default:
            return "Error ";
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

static uint8_t calculate_fb(uint16_t a, uint16_t b)
{
    uint8_t num_b = 0;
    uint8_t num_w = 0;
    uint8_t color_counts_a[MM_NUM_COLORS] = { 0 };
    uint8_t color_counts_b[MM_NUM_COLORS] = { 0 };

    for (uint8_t i = 0; i < MM_NUM_SLOTS; i++)
    {
        Color col_a = code_to_color(a, i);
        Color col_b = code_to_color(b, i);
        color_counts_a[col_a]++;
        color_counts_b[col_b]++;
        if (col_a == col_b)
        {
            num_b++;
        }
    }

    for (uint8_t i = 0; i < MM_NUM_COLORS; i++)
    {
        num_w += MIN(color_counts_a[i], color_counts_b[i]);
    }
    num_w -= num_b;

    return feedback_encode[num_b][num_w];
}

void mm_code_to_feedback(uint8_t code, uint8_t *b, uint8_t *w)
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
    initialized = true;
}

void mm_init_feedback_lookup()
{
    if (fb_lookup_init)
    {
        return;
    }

    for (uint16_t a = 0; a < MM_NUM_INPUTS; a++)
    {
        for (uint16_t b = 0; b <= a; b++)
        {
            feedback_lookup[a][b] = calculate_fb(a, b);
            feedback_lookup[b][a] = feedback_lookup[a][b];
        }
    }
    fb_lookup_init = true;
}

uint8_t mm_get_feedback(uint16_t a, uint16_t b)
{
    if (fb_lookup_init)
    {
        return feedback_lookup[a][b];
    }
    else
    {
        return calculate_fb(a, b);
    }
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
    uint8_t fb = feedback_encode[inp[0] - '0'][inp[1] - '0'];
    printf("\033[F");
    mm_print_feedback(fb);
    printf("\n");
    return fb;
}

uint16_t mm_read_colors()
{
    printf("colors [orybcgpd]*%d: ", MM_NUM_SLOTS);
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
            case 'p':
                colors[i] = MM_COL_PINK;
                break;
            case 'd':
                colors[i] = MM_COL_DARKGREEN;
                break;
        }
    }
    uint16_t result = colors_to_code(colors);
    return result;
}

void mm_print_colors(uint16_t input)
{
    char *str = mm_get_colors_string(input);
    printf("%s", str);
    free(str);
}

char *mm_get_colors_string(uint16_t input)
{
    StringBuilder builder = strbuilder_create(1);
    strbuilder_append(&builder, " ");
    for (uint8_t i = 0; i < MM_NUM_SLOTS; i++)
    {
        strbuilder_append(&builder, "%s  ", col_to_str(code_to_color(input, i))); // 32 chars
    }
    return strbuilder_to_str(&builder);
}

void mm_print_feedback(uint8_t feedback)
{
    uint8_t b, w;
    mm_code_to_feedback(feedback, &b, &w);
    printf(BLK_BG WHT " Black: %d " RST "    " WHT_BG BLK " White: %d " RST, b, w); // 24 chars
}
