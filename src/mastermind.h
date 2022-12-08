#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_MAX_GUESSES   20
#define MAX_NUM_COLORS    10
#define MAX_NUM_SLOTS     10
#define MAX_NUM_CODES     50
#define MAX_NUM_FEEDBACKS 30

typedef enum
{
    MM_STRAT_AVERAGE = 0,
    MM_STRAT_MINMAX,
    MM_NUM_STRATEGIES
} MM_Strategy;

typedef struct MM_Context MM_Context;
typedef struct MM_Match MM_Match;

typedef void MM_PrintFeedbackFunc(uint8_t feedback);
typedef uint16_t MM_ReadCodeFunc();

MM_Context *mm_new_ctx(uint8_t max_guesses, uint8_t num_slots, uint8_t num_colors, const char * const *colors);
void mm_free_ctx(MM_Context *ctx);
uint8_t mm_get_feedback(MM_Context *ctx, uint16_t a, uint16_t b);
void mm_code_to_feedback(MM_Context *ctx, uint8_t fb_code, uint8_t *b, uint8_t *w);
uint16_t mm_feedback_to_code(MM_Context *ctx, uint8_t b, uint8_t w);
bool mm_is_winning_feedback(MM_Context *ctx, uint8_t fb);
uint8_t mm_get_color_at_pos(uint8_t num_colors, uint16_t code, uint8_t index);
uint16_t mm_colors_to_code(MM_Context *ctx, uint8_t *colors);

uint8_t mm_get_max_guesses(MM_Context *ctx);
uint16_t mm_get_num_codes(MM_Context *ctx);
uint8_t mm_get_num_colors(MM_Context *ctx);
uint8_t mm_get_num_slots(MM_Context *ctx);
const char *mm_get_color(MM_Context *ctx, uint8_t index);
char mm_get_color_char(MM_Context *ctx, uint8_t index);

MM_Match *mm_new_match(MM_Context *ctx, bool enable_recommendation);
void mm_free_match(MM_Match *match);
void mm_constrain(MM_Match *match, uint16_t input, uint8_t feedback);
uint16_t mm_recommend(MM_Match *match, MM_Strategy strategy);
MM_Match *mm_play_game(MM_Context *ctx, uint16_t solution, MM_ReadCodeFunc read_code_cbk, MM_PrintFeedbackFunc print_fb_cbk);

uint16_t mm_get_remaining_solutions(MM_Match *match);
uint16_t mm_get_turns(MM_Match *match);
uint8_t mm_get_history_feedback(MM_Match *match, uint8_t index);
uint8_t mm_get_history_guess(MM_Match *match, uint8_t index);