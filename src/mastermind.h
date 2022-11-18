#include <stdint.h>

#define MM_NUM_SLOTS        4
#define MM_NUM_FEEDBACKS   14
#define MM_NUM_INPUTS    1296

typedef enum
{
    MM_COL_ORANGE = 0,
    MM_COL_RED,
    MM_COL_YELLOW,
    MM_COL_BLUE,
    MM_COL_GREEN,
    MM_COL_CYAN,
    MM_NUM_COLORS
} Color;

void mm_init();
uint8_t mm_get_feedback(uint16_t input, uint16_t solution);
bool mm_is_winning_feedback(uint8_t fb);

uint8_t mm_read_feedback();
uint16_t mm_read_input();
void mm_print_colors(uint16_t input);
void mm_print_feedback(uint8_t feedback);
