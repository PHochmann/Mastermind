#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    STRAT_AVERAGE = 0,
    STRAT_MINMAX,
    NUM_STRATEGIES
} Strategy;

typedef struct MasterMind MasterMind;

void mm_init(bool precompute);
MasterMind *mm_get_new_game();
void mm_end_game(MasterMind* mm);

void mm_constrain(MasterMind *mm, uint16_t input, uint8_t feedback);
uint16_t mm_recommend(MasterMind *mm, Strategy strat);
double mm_measure_average(Strategy strat);
bool mm_is_winning_feedback(uint8_t fb);

uint16_t mm_get_remaining_solutions(MasterMind *mm);
uint16_t mm_get_turns(MasterMind *mm);

uint8_t mm_read_feedback();
void mm_print_colors(uint16_t input);
