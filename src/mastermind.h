#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MM_MAX_MAX_GUESSES   20
#define MM_MAX_NUM_COLORS    8
#define MM_MAX_NUM_SLOTS     6
#define MM_MAX_NUM_FEEDBACKS 27 // (MAX_NUM_SLOTS * (MAX_NUM_SLOTS / 2.0 + 1.5))

typedef uint32_t Code_t;
typedef uint32_t CodeSize_t;
typedef uint16_t Feedback_t;
typedef uint16_t FeedbackSize_t;

typedef enum
{
    MM_MATCH_PENDING,
    MM_MATCH_WON,
    MM_MATCH_LOST
} MM_MatchState;

typedef struct MM_Context MM_Context;
typedef struct MM_Match MM_Match;

MM_Context *mm_new_ctx(int max_guesses, int num_slots, int num_colors);
void mm_free_ctx(MM_Context *ctx);
Feedback_t mm_get_feedback(MM_Context *ctx, Code_t a, Code_t b);
void mm_code_to_feedback(MM_Context *ctx, Feedback_t fb_code, int *b, int *w);
Feedback_t mm_feedback_to_code(MM_Context *ctx, int b, int w);
bool mm_is_winning_feedback(MM_Context *ctx, Feedback_t fb);
int mm_get_color_at_pos(int num_colors, Code_t code, int index);
Code_t mm_colors_to_code(MM_Context *ctx, int *colors);

int mm_get_max_guesses(MM_Context *ctx);
CodeSize_t mm_get_num_codes(MM_Context *ctx);
int mm_get_num_colors(MM_Context *ctx);
int mm_get_num_slots(MM_Context *ctx);
int mm_get_num_feedbacks(MM_Context *ctx);

MM_Match *mm_new_match(MM_Context *ctx, bool enable_sol_counting);
void mm_free_match(MM_Match *match);
CodeSize_t mm_constrain(MM_Match *match, Code_t input, Feedback_t feedback);
MM_Context *mm_get_context(MM_Match *match);

CodeSize_t mm_get_remaining_solutions(const MM_Match *match);
int mm_get_turns(const MM_Match *match);
Feedback_t mm_get_history_feedback(const MM_Match *match, int index);
Code_t mm_get_history_guess(const MM_Match *match, int index);
MM_MatchState mm_get_state(const MM_Match *match);
bool mm_is_solution_counting_enabled(const MM_Match *match);
bool mm_is_in_solution(const MM_Match *match, Code_t code);

void mm_init_feedback_lookup(MM_Context *ctx);
