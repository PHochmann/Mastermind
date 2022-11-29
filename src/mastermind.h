#include <stdint.h>

#define MM_NUM_SLOTS            5
#define MM_NUM_COLORS           8
#define MM_NUM_MAX_GUESSES     12

#define MM_NUM_FEEDBACKS       20 // (3+NUM_SLOTS-1) C NUM_SLOTS - 1
#define MM_NUM_INPUTS       32768 // NUM_COLORS^NUM_SLOTS

// Todo: Replace defines by dynamic options struct
/*typedef struct
{
    uint8_t num_slots;
    uint8_t num_colors;
    uint8_t max_guesses;

    // Computed:
    uint16_t num_feedbacks;
    uint16_t num_inputs;
} MM_Rules;*/

void mm_init();
void mm_init_feedback_lookup();
uint8_t mm_get_feedback(uint16_t a, uint16_t b);
bool mm_is_winning_feedback(uint8_t fb);

uint8_t mm_read_feedback();
uint16_t mm_read_colors();
void mm_print_colors(uint16_t input);
void mm_print_feedback(uint8_t feedback);
char *mm_get_colors_string(uint16_t input);
void mm_code_to_feedback(uint8_t code, uint8_t *b, uint8_t *w);
