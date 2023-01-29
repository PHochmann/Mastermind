#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <readline/readline.h>
#include <inttypes.h>
#include <errno.h>

#include "console.h"
#include "../multiplayer/protocol.h"
#include "string_builder.h"
#include "string_util.h"
#include "table.h"


typedef struct
{
    const char *str;
    const char *col;
} Color;

static const Color colors[] = {
    { "Orange", "\033[38:2:255:165:000m"},
    { " Red  ", "\033[38:2:255:000:000m" },
    { "Yellow", "\033[38:2:250:237:000m" },
    { " Blue ", "\033[38:2:000:000:255m" },
    { " Cyan ", "\033[38:2:065:253:254m" },
    { "Green ", "\033[38:2:000:255:000m" },
    { "DrkGrn", "\033[38:2:085:107:047m" },
    { "Pink  ", "\033[38:2:219:112:147m" },
};

#define FANCY_FEEDBACK 1

#define GRE    "\033[38:2:100:100:100m"
#define GRE_BG "\033[48:2:100:100:100m"
#define BLK    "\033[38:2:000:000:000m"
#define WHT    "\033[38:2:255:255:255m"
#define BLK_BG "\033[48:2:000:000:000m"
#define WHT_BG "\033[48:2:255:255:255m"
#define RST    "\033[0m"

// Static functions

/*
static const char *get_col_str(int index)
{

}

static const char *get_col_prefix(int index)
{

}
*/

// Messages

void clear_input()
{
    printf("\x1b[1F"); // Move to beginning of previous line
    printf("\x1b[2K"); // Clear entire line
}

void clear_screen()
{
    printf("\033[1;1H\033[2J");
}

void print_winning_message(int num_turns)
{
    printf("~ ~ You guessed right! You took %d guesses. ~ ~\n", num_turns);
}

void print_losing_message(MM_Context *ctx, Code_t solution)
{
    printf("~ ~ Game over. You couldn't make it in %d turns. ~ ~\n", mm_get_max_guesses(ctx));
    printf("Solution: ");
    print_colors(ctx, solution);
    printf("\n");
}

// IO Functions

char *readline_fmt(const char *fmt, ...)
{
    StringBuilder builder = strb_create();
    va_list args;
    va_start(args, fmt);
    vstrb_append(&builder, fmt, args);
    va_end(args);
    char *result = readline(strb_to_str(&builder));
    strb_destroy(&builder);
    return result;
}

Feedback_t read_feedback(MM_Context *ctx)
{
    StringBuilder pb = strb_create();
    strb_append(&pb, "#blacks, #whites: ");

    bool validated = false;
    char *input    = NULL;

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        if (input == NULL)
        {
            continue;
        }
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

bool read_colors(MM_Context *ctx, int turn, Code_t *out_code)
{
    StringBuilder pb = strb_create();
    strb_append(&pb, "%d/%d colors [", turn, mm_get_max_guesses(ctx));
    for (int i = 0; i < mm_get_num_colors(ctx); i++)
    {
        strb_append(&pb, "%s%c" RST, colors[i].col, to_lower(*first_char(colors[i].str)));
    }
    strb_append(&pb, "]*%d: ", mm_get_num_slots(ctx));

    bool validated = false;
    char *input    = NULL;
    int input_colors[MAX_NUM_SLOTS];

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        clear_input();
        if (input == NULL)
        {
            return false;
        }

        if ((int)strlen(input) == mm_get_num_slots(ctx))
        {
            validated = true;
            for (int i = 0; (i < mm_get_num_slots(ctx)) && validated; i++)
            {
                input_colors[i] = UINT8_MAX;
                for (int j = 0; j < mm_get_num_colors(ctx); j++)
                {
                    if (to_lower(*first_char(colors[j].str)) == to_lower(input[i]))
                    {
                        input_colors[i] = j;
                        break;
                    }
                }
                if (input_colors[i] == UINT8_MAX)
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
    *out_code = mm_colors_to_code(ctx, input_colors);
    print_colors(ctx, *out_code);
    strb_destroy(&pb);
    return true;
}

void print_colors(MM_Context *ctx, Code_t input)
{
    char *str = get_colors_string(ctx, input);
    printf("%s", str);
    free(str);
}

char *get_colors_string(MM_Context *ctx, Code_t input)
{
    StringBuilder builder = strb_create();
    strb_append(&builder, " ");
    for (int i = 0; i < mm_get_num_slots(ctx); i++)
    {
        int col = mm_get_color_at_pos(mm_get_num_colors(ctx), input, i);
        strb_append(&builder, "%s%s" RST "  ", colors[col].col, colors[col].str);
    }
    return strb_to_str(&builder);
}

void print_feedback(MM_Context *ctx, Feedback_t feedback)
{
    int b, w;
    mm_code_to_feedback(ctx, feedback, &b, &w);
    if (FANCY_FEEDBACK)
    {
        printf(BLK_BG WHT);
        for (int i = 0; i < b; i++)
        {
            printf(" X ");
        }
        printf(WHT_BG BLK);
        for (int i = 0; i < w; i++)
        {
            printf(" x ");
        }
        printf(RST);
        for (int i = 0; i < mm_get_num_slots(ctx) - b - w; i++)
        {
            printf(" . ");
        }
    }
    else
    {
        printf(BLK_BG WHT " Black: %d " RST "    " WHT_BG BLK " White: %d " RST, b, w); // 24 chars   
    }
}

void print_round_summary_table(MM_Context *ctx,
                               int num_players,
                               char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES],
                               int turns[MAX_NUM_PLAYERS],
                               Code_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES],
                               Code_t solution,
                               int round,
                               int points[MAX_NUM_PLAYERS])
{
    int max_turns = 0;
    for (int i = 0; i < num_players; i++)
    {
        if (turns[i] > max_turns)
        {
            max_turns = turns[i];
        }
    }

    Table *tbl = get_empty_table();
    set_span(tbl, num_players * 3, 1);
    override_horizontal_alignment(tbl, H_ALIGN_CENTER);
    add_cell_fmt(tbl, "Summary of round %d", round);
    next_row(tbl);
    set_hline(tbl, BORDER_SINGLE);

    for (int i = 0; i < num_players; i++)
    {
        add_cell_fmt(tbl, " %s ", names[i]);
        add_cell(tbl, "#B");
        add_cell(tbl, "#W ");
    }

    next_row(tbl);
    set_hline(tbl, BORDER_SINGLE);

    for (int i = 0; i < max_turns + 1; i++)
    {
        for (int j = 0; j < num_players; j++)
        {
            if (i < turns[j])
            {
                add_cell_gc(tbl, get_colors_string(ctx, guesses[j][i]));
                int b, w;
                mm_code_to_feedback(ctx, mm_get_feedback(ctx, guesses[j][i], solution), &b, &w);
                add_cell_fmt(tbl, " %d ", b);
                add_cell_fmt(tbl, " %d ", w);
            }
            else
            {
                add_empty_cell(tbl);
                add_empty_cell(tbl);
                add_empty_cell(tbl);
            }
        }
        next_row(tbl);
    }
    set_hline(tbl, BORDER_SINGLE);
    for (int i = 0; i < num_players; i++)
    {
        set_span(tbl, 3, 1);
        add_cell_fmt(tbl, " %d points total ", points[i]);
    }
    next_row(tbl);

    for (int i = 1; i < num_players; i++)
    {
        set_vline(tbl, i * 3, BORDER_SINGLE);
    }

    make_boxed(tbl, BORDER_SINGLE);
    print_table(tbl);
    free_table(tbl);
}

bool readline_int(const char *prompt, int default_value, int min, int max, int *result)
{
    StringBuilder builder = strb_create();
    strb_append(&builder, "%s", prompt);
    strb_append(&builder, " (%d-%d, default: %d): ", min, max, default_value);
    bool validated = false;
    while (!validated)
    {
        char *input = readline(strb_to_str(&builder));
        clear_input();
        if (input == NULL)
        {
            return false;
        }
        if (input[0] == '\0')
        {
            free(input);
            *result = default_value;
            return true;
        }
        *result = strtol(input, NULL, 10);
        if ((errno != ERANGE) && (*result >= min) && (*result <= max))
        {
            validated = true;
        }
        free(input);
    }
    strb_destroy(&builder);
    return true;
}
