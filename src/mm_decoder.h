#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    MM_STRAT_AVERAGE = 0,
    MM_STRAT_MINMAX,
    MM_NUM_STRATEGIES
} Strategy;

typedef struct MM_Decoder MM_Decoder;


MM_Decoder *mm_new_decoder();
void mm_end_decoder(MM_Decoder* mm);
void mm_init_recommendation_lookup();

void mm_constrain(MM_Decoder *mm, uint16_t input, uint8_t feedback);
uint16_t mm_recommend(MM_Decoder *mm, Strategy strat);
double mm_measure_average(Strategy strat);

uint16_t mm_get_remaining_solutions(MM_Decoder *mm);
uint16_t mm_get_turns(MM_Decoder *mm);
