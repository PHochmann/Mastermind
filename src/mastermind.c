#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "mastermind.h"
#include "util/string_util.h"

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a >= b ? a : b)

struct MM_Context
{
    int max_guesses;
    int num_slots;
    int num_colors;
    FeedbackSize_t num_feedbacks;
    CodeSize_t num_codes;
    Feedback_t feedback_encode[MAX_NUM_SLOTS + 1][MAX_NUM_SLOTS + 1];
    uint16_t feedback_decode[MAX_NUM_FEEDBACKS];

    // Optional
    bool fb_lookup_initialized;
    Feedback_t *feedback_lookup; // On heap
};

struct MM_Match
{
    MM_Context *ctx;
    int num_turns;
    Feedback_t feedbacks[MAX_MAX_GUESSES];
    Code_t guesses[MAX_MAX_GUESSES];
    bool enable_recommendation;
    CodeSize_t num_solutions;
    bool *solution_space; // On heap
};

static Feedback_t calculate_fb(MM_Context *ctx, Code_t a, Code_t b)
{
    int num_b                          = 0;
    int num_w                          = 0;
    int color_counts_a[MAX_NUM_COLORS] = { 0 };
    int color_counts_b[MAX_NUM_COLORS] = { 0 };

    for (int i = 0; i < ctx->num_slots; i++)
    {
        int col_a = mm_get_color_at_pos(ctx->num_colors, a, i);
        int col_b = mm_get_color_at_pos(ctx->num_colors, b, i);
        color_counts_a[col_a]++;
        color_counts_b[col_b]++;
        if (col_a == col_b)
        {
            num_b++;
        }
    }

    for (int i = 0; i < ctx->num_colors; i++)
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
    ctx->feedback_lookup = malloc(ctx->num_codes * ctx->num_codes * sizeof(Code_t));
    for (CodeSize_t a = 0; a < ctx->num_codes; a++)
    {
        for (CodeSize_t b = 0; b <= a; b++)
        {
            ctx->feedback_lookup[a * ctx->num_codes + b] = calculate_fb(ctx, a, b);
            ctx->feedback_lookup[b * ctx->num_codes + a] = ctx->feedback_lookup[a * ctx->num_codes + b];
        }
    }
    ctx->fb_lookup_initialized = true;
}

/*
 * PUBLIC FUNCTIONS
 *
 *
 */

MM_Match *mm_new_match(MM_Context *ctx, bool enable_recommendation)
{
    MM_Match *result = malloc(sizeof(MM_Match));

    *result = (MM_Match){ .ctx                   = ctx,
                          .solution_space        = NULL,
                          .num_turns             = 0,
                          .num_solutions         = 0,
                          .enable_recommendation = enable_recommendation };

    if (enable_recommendation)
    {
        // init_feedback_lookupinit_feedback_lookup(ctx);
        result->num_solutions  = ctx->num_codes;
        result->solution_space = malloc(ctx->num_codes * sizeof(bool));
        if (result->solution_space == NULL)
        {
            result->enable_recommendation = false;
            return result;
        }
        for (CodeSize_t i = 0; i < ctx->num_codes; i++)
        {
            result->solution_space[i] = true;
        }
    }
    return result;
}

MM_Context *mm_new_ctx(int max_guesses, int num_slots, int num_colors)
{
    FeedbackSize_t num_feedbacks = num_slots * (num_slots / 2.0 + 1.5);

    if ((num_slots > MAX_NUM_SLOTS) || (num_colors > MAX_NUM_COLORS) || (num_feedbacks > MAX_NUM_FEEDBACKS) || (max_guesses > MAX_MAX_GUESSES))
    {
        return NULL;
    }

    MM_Context *ctx = malloc(sizeof(MM_Context));
    *ctx            = (MM_Context){ .max_guesses   = max_guesses,
                                    .num_slots     = num_slots,
                                    .num_colors    = num_colors,
                                    .num_feedbacks = num_feedbacks,
                                    .num_codes     = pow(num_colors, num_slots) };

    FeedbackSize_t counter = 0;
    for (int b = 0; b <= num_slots; b++)
    {
        for (int w = 0; w <= num_slots; w++)
        {
            if ((b + w) <= num_slots && !(b == num_slots - 1 && w == 1))
            {
                ctx->feedback_encode[b][w]    = counter;
                ctx->feedback_decode[counter] = (b << 8) | w;
                counter++;
            }
        }
    }

    return ctx;
}

void mm_free_ctx(MM_Context *ctx)
{
    if (ctx->fb_lookup_initialized)
    {
        free(ctx->feedback_lookup);
    }
    free(ctx);
}

void mm_code_to_feedback(MM_Context *ctx, Feedback_t fb_code, int *b, int *w)
{
    *b = ctx->feedback_decode[fb_code] >> 8;
    *w = ctx->feedback_decode[fb_code] & 0xFF;
}

Feedback_t mm_feedback_to_code(MM_Context *ctx, int b, int w)
{
    return ctx->feedback_encode[b][w];
}

Feedback_t mm_get_feedback(MM_Context *ctx, Code_t a, Code_t b)
{
    if (ctx->fb_lookup_initialized)
    {
        return ctx->feedback_lookup[a * ctx->num_codes + b];
    }
    else
    {
        return calculate_fb(ctx, a, b);
    }
}

bool mm_is_winning_feedback(MM_Context *ctx, Feedback_t fb)
{
    return fb == ctx->feedback_encode[ctx->num_slots][0];
}

Code_t mm_colors_to_code(MM_Context *ctx, int *colors)
{
    Code_t result = 0;
    for (int i = 0; i < mm_get_num_slots(ctx); i++)
    {
        result += colors[i] * pow(mm_get_num_colors(ctx), i);
    }
    return result;
}

int mm_get_color_at_pos(int num_colors, Code_t code, int index)
{
    return (int)(code / pow(num_colors, index)) % num_colors;
}

int mm_get_max_guesses(MM_Context *ctx)
{
    return ctx->max_guesses;
}

CodeSize_t mm_get_num_codes(MM_Context *ctx)
{
    return ctx->num_codes;
}

int mm_get_num_slots(MM_Context *ctx)
{
    return ctx->num_slots;
}

int mm_get_num_colors(MM_Context *ctx)
{
    return ctx->num_colors;
}

void mm_free_match(MM_Match *match)
{
    free(match->solution_space);
    free(match);
}

Code_t mm_recommend_guess(MM_Match *match, Code_t solution)
{
    if (!match->enable_recommendation)
    {
        return 0;
    }
    init_feedback_lookup(match->ctx);
    long *aggregations = malloc(match->ctx->num_codes * sizeof(long));

    if (match->num_solutions == 1)
    {
        for (Code_t j = 0; j < match->ctx->num_codes; j++)
        {
            if (match->solution_space[j])
            {
                return j;
            }
        }
    }

    for (Code_t i = 0; i < match->ctx->num_codes; i++)
    {
        aggregations[i] = 0;
        for (Feedback_t fb = 0; fb < match->ctx->num_feedbacks; fb++)
        {
            CodeSize_t num_solutions = 0;
            for (Code_t j = 0; j < match->ctx->num_codes; j++)
            {
                if (match->solution_space[j])
                {
                    if (mm_get_feedback(match->ctx, j, i) == fb)
                    {
                        num_solutions++;
                    }
                }
            }
            aggregations[i] = MAX(aggregations[i], num_solutions);
        }
    }

    Code_t min = 0;
    for (Code_t i = 0; i < match->ctx->num_codes; i++)
    {
        if (aggregations[i] < aggregations[min])
        {
            Feedback_t fb = mm_get_feedback(match->ctx, i, solution);
            int b, w;
            mm_code_to_feedback(match->ctx, fb, &b, &w);
            int fb_score = 2 * b + w;
            if (fb_score < match->ctx->num_slots / 2)
            {
                min = i;
            }
        }
    }

    free(aggregations);
    return min;
}

CodeSize_t mm_constrain(MM_Match *match, Code_t guess, Feedback_t feedback)
{
    match->guesses[match->num_turns]   = guess;
    match->feedbacks[match->num_turns] = feedback;
    match->num_turns++;
    CodeSize_t result = 0;

    if (match->enable_recommendation)
    {
        CodeSize_t remaining = 0;
        for (CodeSize_t i = 0; i < match->ctx->num_codes; i++)
        {
            if (match->solution_space[i])
            {
                if (mm_get_feedback(match->ctx, guess, i) != feedback)
                {
                    match->solution_space[i] = false;
                    result++;
                }
                else
                {
                    remaining++;
                }
            }
        }
        // Only reduce remaining solutions if not winning feedback
        if (!mm_is_winning_feedback(match->ctx, feedback))
        {
            match->num_solutions = remaining;
        }
    }

    return result;
}

MM_Context *mm_get_context(MM_Match *match)
{
    return match->ctx;
}

CodeSize_t mm_get_remaining_solutions(MM_Match *match)
{
    if (!match->enable_recommendation)
    {
        return UINT16_MAX;
    }
    return match->num_solutions;
}

int mm_get_turns(MM_Match *match)
{
    return match->num_turns;
}

Feedback_t mm_get_history_feedback(MM_Match *match, int index)
{
    return match->feedbacks[index];
}

Code_t mm_get_history_guess(MM_Match *match, int index)
{
    return match->guesses[index];
}

MM_MatchState mm_get_state(MM_Match *match)
{
    if ((match->num_turns != 0) && (mm_is_winning_feedback(match->ctx, match->feedbacks[match->num_turns - 1])))
    {
        return MM_MATCH_WON;
    }
    else if (match->num_turns >= match->ctx->max_guesses)
    {
        return MM_MATCH_LOST;
    }
    else
    {
        return MM_MATCH_PENDING;
    }
}

bool mm_is_solution_counting_enabled(MM_Match *match)
{
    return match->enable_recommendation;
}

bool *mm_get_solution_space(MM_Match *match)
{
    return match->solution_space;
}
