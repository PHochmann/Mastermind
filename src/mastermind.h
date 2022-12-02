#include <stdint.h>

typedef enum
{
    MM_STRAT_AVERAGE = 0,
    MM_STRAT_MINMAX,
    MM_NUM_STRATEGIES
} MM_Strategy;

typedef struct MM_Context MM_Context;
typedef struct MM_Match MM_Match;

MM_Context *mm_new_ctx(uint8_t max_guesses, uint8_t num_slots, uint8_t num_colors, const char * const *colors);
void mm_free_ctx(MM_Context *ctx);
void mm_code_to_feedback(MM_Context *ctx, uint8_t fb_code, uint8_t *b, uint8_t *w);
uint8_t mm_get_feedback(MM_Context *ctx, uint16_t a, uint16_t b);
bool mm_is_winning_feedback(MM_Context *ctx, uint8_t fb);

uint8_t mm_read_feedback(MM_Context *ctx);
uint16_t mm_read_colors(MM_Context *ctx, uint8_t turn);
void mm_print_colors(MM_Context *ctx, uint16_t input);
void mm_print_feedback(MM_Context *ctx, uint8_t feedback);
char *mm_get_colors_string(MM_Context *ctx, uint16_t input);

uint8_t mm_get_max_guesses(MM_Context *ctx);
uint16_t mm_get_num_inputs(MM_Context *ctx);


// Match
MM_Match *mm_new_match(MM_Context *ctx, bool enable_recommendation);
void mm_free_match(MM_Match *match);
void mm_constrain(MM_Match *match, uint16_t input, uint8_t feedback);
uint16_t mm_recommend(MM_Match *match, MM_Strategy strategy);
uint16_t mm_get_remaining_solutions(MM_Match *match);
uint16_t mm_get_turns(MM_Match *match);
