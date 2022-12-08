#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <readline/readline.h>

#include "console.h"
#include "string_builder.h"
#include "string_util.h"

#define BLK     "\033[38:2:000:000:000m"
#define WHT     "\033[38:2:255:255:255m"
#define BLK_BG  "\033[48:2:000:000:000m"
#define WHT_BG  "\033[48:2:255:255:255m"
#define RST     "\033[0m"

void clear_input()
{
    printf("\x1b[1F"); // Move to beginning of previous line
    printf("\x1b[2K"); // Clear entire line
}

void clear_screen()
{
    printf("\033[1;1H\033[2J");
}

void any_key()
{
    printf("Press enter to continue...\n");
    getchar();
}

uint8_t digit_to_uint(char c)
{
    return c - '0';
}

void print_winning_message(uint8_t num_turns)
{
    printf("~ ~ Game won in %d steps ~ ~\n\n", num_turns);
}

void print_losing_message(uint8_t num_max_guesses)
{
    printf("~ ~ Game over - You could not make it in %d turns ~ ~\n", num_max_guesses);
}

// IO Functions

uint8_t read_feedback(MM_Context *ctx)
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

    return mm_feedback_to_code(ctx, input[0] - '0', input[1] - '0');
}

uint16_t read_colors(MM_Context *ctx, uint8_t turn)
{
    StringBuilder pb = strb_create();
    strb_append(&pb, "%d/%d colors [", turn + 1, mm_get_max_guesses(ctx));
    for (uint8_t i = 0; i < mm_get_num_colors(ctx); i++)
    {
        strb_append(&pb, "%c", mm_get_color_char(ctx, i));
    }
    strb_append(&pb, "]*%d: ", mm_get_num_slots(ctx));

    bool validated = false;
    char *input = NULL;
    uint8_t colors[MAX_NUM_COLORS];

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        clear_input();

        if (strlen(input) == mm_get_num_slots(ctx))
        {
            validated = true;
            for (uint8_t i = 0; (i < mm_get_num_slots(ctx)) && validated; i++)
            {
                colors[i] = UINT8_MAX;
                for (uint8_t j = 0; j < mm_get_num_colors(ctx); j++)
                {
                    if (mm_get_color_char(ctx, j) == to_lower(input[i]))
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

    uint16_t result = mm_colors_to_code(ctx, colors);
    strb_destroy(&pb);
    return result;
}

void print_colors(MM_Context *ctx, uint16_t input)
{
    char *str = get_colors_string(ctx, input);
    printf("%s", str);
    free(str);
}

char *get_colors_string(MM_Context *ctx, uint16_t input)
{
    StringBuilder builder = strb_create();
    strb_append(&builder, " ");
    for (uint8_t i = 0; i < mm_get_num_slots(ctx); i++)
    {
        strb_append(&builder, "%s  ", mm_get_color(ctx, mm_get_color_at_pos(mm_get_num_colors(ctx), input, i)));
    }
    return strb_to_str(&builder);
}

void print_feedback(MM_Context *ctx, uint8_t feedback)
{
    uint8_t b, w;
    mm_code_to_feedback(ctx, feedback, &b, &w);
    printf(BLK_BG WHT " Black: %d " RST "    " WHT_BG BLK " White: %d " RST, b, w); // 24 chars
}
