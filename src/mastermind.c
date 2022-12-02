#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <readline/readline.h>

#include "mastermind.h"
#include "util/console.h"
#include "util/string_util.h"
#include "util/string_builder.h"

#define BLK     "\033[38:2:000:000:000m"
#define WHT     "\033[38:2:255:255:255m"
#define BLK_BG  "\033[48:2:000:000:000m"
#define WHT_BG  "\033[48:2:255:255:255m"
#define RST     "\033[0m"

#define MAX_NUM_COLORS    10
#define MAX_NUM_SLOTS     10
#define MAX_NUM_INPUTS    50
#define MAX_NUM_FEEDBACKS 30

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a >= b ? a : b)

struct MM_Context
{
    uint8_t max_guesses;
    uint8_t num_slots;
    uint8_t num_colors;
    uint8_t num_feedbacks;
    uint16_t num_inputs;
    const char * const *colors;
    char color_chars[MAX_NUM_COLORS];
    uint8_t feedback_encode[MAX_NUM_SLOTS + 1][MAX_NUM_SLOTS + 1];
    uint16_t feedback_decode[MAX_NUM_FEEDBACKS];

    // Optional
    bool fb_lookup_initialized;
    uint8_t *feedback_lookup; // On heap

    // Recommender
    bool lookups_init;
    uint16_t recomm_step1_lookup[MM_NUM_STRATEGIES];
    bool recomm_step1_init[MM_NUM_STRATEGIES];
    uint16_t recomm_step2_lookup[MM_NUM_STRATEGIES][MAX_NUM_FEEDBACKS];
    bool recomm_step2_init[MM_NUM_STRATEGIES];
};

struct MM_Match
{
    MM_Context *ctx;
    uint8_t num_turns;
    uint8_t last_feedback;

    bool enable_recommendation;
    uint16_t num_solutions;
    bool *solution_space;
};

static uint8_t code_to_color(uint8_t num_colors, uint16_t code, uint8_t index)
{
    return (int)(code / pow(num_colors, index)) % num_colors;
}

static uint16_t colors_to_code(uint8_t num_slots, uint8_t num_colors, uint8_t *colors)
{
    uint16_t result = 0;
    for (uint8_t i = 0; i < num_slots; i++)
    {
        result += colors[i] * pow(num_colors, i);
    }
    return result;
}

static uint8_t calculate_fb(MM_Context *ctx, uint16_t a, uint16_t b)
{
    uint8_t num_b = 0;
    uint8_t num_w = 0;
    uint8_t color_counts_a[MAX_NUM_COLORS] = { 0 };
    uint8_t color_counts_b[MAX_NUM_COLORS] = { 0 };

    for (uint8_t i = 0; i < ctx->num_slots; i++)
    {
        uint8_t col_a = code_to_color(ctx->num_colors, a, i);
        uint8_t col_b = code_to_color(ctx->num_colors, b, i);
        color_counts_a[col_a]++;
        color_counts_b[col_b]++;
        if (col_a == col_b)
        {
            num_b++;
        }
    }

    for (uint8_t i = 0; i < ctx->num_colors; i++)
    {
        num_w += MIN(color_counts_a[i], color_counts_b[i]);
    }
    num_w -= num_b;

    return ctx->feedback_encode[num_b][num_w];
}

void init_feedback_lookup(MM_Context *ctx)
{
    if (ctx->fb_lookup_initialized)
    {
        return;
    }

    ctx->feedback_lookup = malloc(ctx->num_inputs * ctx->num_inputs);

    for (uint16_t a = 0; a < ctx->num_inputs; a++)
    {
        for (uint16_t b = 0; b <= a; b++)
        {
            ctx->feedback_lookup[a * ctx->num_inputs + b] = calculate_fb(ctx, a, b);
            ctx->feedback_lookup[b * ctx->num_inputs + a] = ctx->feedback_lookup[a * ctx->num_inputs + b];
        }
    }
    ctx->fb_lookup_initialized = true;
}

void init_recommendation_lookup(MM_Context *ctx)
{
    /* We build a lookup table for the first and second recommendation.
     * This drastically improves performance since computation time for mm_recommend()
     * decreases a lot for further guesses because of the smaller solution space.
     */
    for (uint8_t strat = 0; strat < MM_NUM_STRATEGIES; strat++)
    {
        // 1. recommendation
        MM_Match *match = mm_new_match(ctx, true);
        ctx->recomm_step1_lookup[strat] = mm_recommend(match, strat);
        ctx->recomm_step1_init[strat] = true;
        mm_free_match(match);

        // 2. recommendation
        for (uint8_t fb = 0; fb < ctx->num_feedbacks; fb++)
        {
            MM_Match *match = mm_new_match(ctx, true);
            mm_constrain(match, mm_recommend(match, strat), fb);
            ctx->recomm_step2_lookup[strat][fb] = mm_recommend(match, strat);
            mm_free_match(match);
        }
        ctx->recomm_step2_init[strat] = true;
    }
}

void init_lookups(MM_Context *ctx)
{
    if (ctx->lookups_init)
    {
        return;
    }

    ctx->lookups_init = true;
    printf("Initializing data structures for recommendation, this may take a while...\n");
    init_feedback_lookup(ctx);
    init_recommendation_lookup(ctx);
}

/*
 * PUBLIC FUNCTIONS
 * 
 * 
*/

MM_Context *mm_new_ctx(uint8_t max_guesses, uint8_t num_slots, uint8_t num_colors, const char * const *colors)
{
    uint8_t num_feedbacks = num_slots * (num_slots / 2.0 + 1.5);

    if (num_slots > MAX_NUM_SLOTS || num_colors > MAX_NUM_COLORS || num_feedbacks > MAX_NUM_FEEDBACKS)
    {
        return NULL;
    }

    MM_Context *ctx = malloc(sizeof(MM_Context));
    *ctx = (MM_Context){
        .max_guesses       = max_guesses,
        .num_slots         = num_slots,
        .num_colors        = num_colors,
        .colors            = colors,
        .num_feedbacks     = num_feedbacks,
        .num_inputs        = pow(num_colors, num_slots),
        .lookups_init      = false,
        .recomm_step1_init = { false },
        .recomm_step2_init = { false }
    };

    uint8_t counter = 0;
    for (uint8_t b = 0; b <= num_slots; b++)
    {
        for (uint8_t w = 0; w <= num_slots; w++)
        {
            if ((b + w) <= num_slots && !(b == num_slots - 1 && w == 1))
            {
                ctx->feedback_encode[b][w] = counter;
                ctx->feedback_decode[counter] = (b << 8) | w;
                counter++;
            }
        }
    }

    for (uint8_t i = 0; i < num_colors; i++)
    {
        ctx->color_chars[i] = to_lower(*first_char(colors[i]));
    }

    return ctx;
}

void mm_free_ctx(MM_Context *ctx)
{
    if (ctx->lookups_init)
    {
        free(ctx->feedback_lookup);
    }
    free(ctx);
}

void mm_code_to_feedback(MM_Context *ctx, uint8_t fb_code, uint8_t *b, uint8_t *w)
{
    *b = ctx->feedback_decode[fb_code] >> 8;
    *w = ctx->feedback_decode[fb_code] & 0xFF;
}

uint8_t mm_get_feedback(MM_Context *ctx, uint16_t a, uint16_t b)
{
    if (ctx->fb_lookup_initialized)
    {
        return ctx->feedback_lookup[a * ctx->num_inputs + b];
    }
    else
    {
        return calculate_fb(ctx, a, b);
    }
}

bool mm_is_winning_feedback(MM_Context *ctx, uint8_t fb)
{
    return fb == ctx->feedback_encode[ctx->num_slots][0];
}

uint8_t mm_get_max_guesses(MM_Context *ctx)
{
    return ctx->max_guesses;
}

uint16_t mm_get_num_inputs(MM_Context *ctx)
{
    return ctx->num_inputs;
}

MM_Match *mm_new_match(MM_Context *ctx, bool enable_recommendation)
{
    MM_Match *result = malloc(sizeof(MM_Match));

    *result = (MM_Match){
        .ctx            = ctx,
        .solution_space = NULL,
        .num_turns      = 0,
        .num_solutions  = 0,
        .enable_recommendation = enable_recommendation
    };

    if (enable_recommendation)
    {
        init_lookups(ctx);
        result->num_solutions = ctx->num_inputs;
        result->solution_space = malloc(ctx->num_inputs * sizeof(bool));
        for (uint16_t i = 0; i < ctx->num_inputs; i++)
        {
            result->solution_space[i] = true;
        }
    }
    return result;
}

void mm_free_match(MM_Match* match)
{
    free(match->solution_space);
    free(match);
}

uint16_t mm_recommend(MM_Match *match, MM_Strategy strat)
{
    if (match->ctx->recomm_step1_init[strat] && match->num_turns == 0)
    {
        return match->ctx->recomm_step1_lookup[strat];
    }
    else if (match->ctx->recomm_step2_init[strat] && match->num_turns == 1)
    {
        return match->ctx->recomm_step2_lookup[strat][match->last_feedback];
    }

    uint32_t aggregations[MAX_NUM_INPUTS];
    for (uint16_t i = 0; i < match->ctx->num_inputs; i++)
    {
        aggregations[i] = UINT32_MAX;
    }

    for (uint16_t i = 0; i < match->ctx->num_inputs; i++)
    {
        if (match->solution_space[i])
        {
            aggregations[i] = 0;
            for (uint16_t j = 0; j < match->ctx->num_inputs; j++)
            {
                uint8_t fb = mm_get_feedback(match->ctx, j, i);
                uint16_t num_solutions = 0;
                for (uint16_t k = 0; k < match->ctx->num_inputs; k++)
                {
                    if (match->solution_space[k])
                    {
                        if (mm_get_feedback(match->ctx, j, k) == fb)
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
    for (uint16_t i = 0; i < match->ctx->num_inputs; i++)
    {
        if (aggregations[i] < aggregations[min])
        {
            min = i;
        }
    }

    return min;
}

void mm_constrain(MM_Match* match, uint16_t input, uint8_t feedback)
{
    if (match->enable_recommendation)
    {
        uint16_t remaining = 0;
        for (uint16_t i = 0; i < match->ctx->num_inputs; i++)
        {
            if (match->solution_space[i])
            {
                if (mm_get_feedback(match->ctx, input, i) != feedback)
                {
                    match->solution_space[i] = false;
                }
                else
                {
                    remaining++;
                }
            }
        }
        match->num_solutions = remaining;
    }

    match->num_turns++;
    match->last_feedback = feedback;
}

uint16_t mm_get_remaining_solutions(MM_Match *match)
{
    if (!match->enable_recommendation)
    {
        return UINT16_MAX;
    }

    return match->num_solutions;
}

uint16_t mm_get_turns(MM_Match *match)
{
    return match->num_turns;
}

// IO Functions

uint8_t mm_read_feedback(MM_Context *ctx)
{
    StringBuilder pb = strb_create();
    strb_append(&pb, "#blacks, #whites: ");

    bool validated = false;
    char *input = NULL;

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        clear_input();

        if (strlen(input) == 2)
        {
            validated = true;
        }
        else
        {
            validated = false;
        }
    }

    return ctx->feedback_encode[input[0] - '0'][input[1] - '0'];
}

uint16_t mm_read_colors(MM_Context *ctx, uint8_t turn)
{
    StringBuilder pb = strb_create();
    strb_append(&pb, "%d/%d colors [", turn + 1, ctx->max_guesses);
    for (uint8_t i = 0; i < ctx->num_colors; i++)
    {
        strb_append(&pb, "%c", ctx->color_chars[i]);
    }
    strb_append(&pb, "]*%d: ", ctx->num_slots);

    bool validated = false;
    char *input = NULL;
    uint8_t colors[MAX_NUM_COLORS];

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        clear_input();

        if (strlen(input) == ctx->num_slots)
        {
            validated = true;
            for (uint8_t i = 0; (i < ctx->num_slots) && validated; i++)
            {
                colors[i] = UINT8_MAX;
                for (uint8_t j = 0; j < ctx->num_colors; j++)
                {
                    if (ctx->color_chars[j] == to_lower(input[i]))
                    {
                        colors[i] = j;
                        break;
                    }
                }
                if (colors[i] == UINT8_MAX)
                {
                    validated = false;
                    break;
                }
            }
        }
        else
        {
            validated = false;
        }

        free(input);
    }

    uint16_t result = colors_to_code(ctx->num_slots, ctx->num_colors, colors);
    strb_destroy(&pb);
    return result;
}

void mm_print_colors(MM_Context *ctx, uint16_t input)
{
    char *str = mm_get_colors_string(ctx, input);
    printf("%s", str);
    free(str);
}

char *mm_get_colors_string(MM_Context *ctx, uint16_t input)
{
    StringBuilder builder = strb_create();
    strb_append(&builder, " ");
    for (uint8_t i = 0; i < ctx->num_slots; i++)
    {
        strb_append(&builder, "%s  ", ctx->colors[code_to_color(ctx->num_colors, input, i)]);
    }
    return strb_to_str(&builder);
}

void mm_print_feedback(MM_Context *ctx, uint8_t feedback)
{
    uint8_t b, w;
    mm_code_to_feedback(ctx, feedback, &b, &w);
    printf(BLK_BG WHT " Black: %d " RST "    " WHT_BG BLK " White: %d " RST, b, w); // 24 chars
}
