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
    { "Orange", "\033[38;5;208m" },
    { " Red  ", "\033[38;5;196m" },
    { "Yellow", "\033[38;5;226m" },
    { " Blue ", "\033[38;5;21m" },
    { " Cyan ", "\033[38;5;51m" },
    { "Green ", "\033[38;5;40m" },
    { "DrkGrn", "\033[38;5;22m" },
    { " Pink ", "\033[38;5;13m" },
};

#define BLK    "\033[38;5;0m"
#define WHT    "\033[38;5;15m"
#define BLK_BG "\033[48;5;0m"
#define WHT_BG "\033[48;5;15m"
#define RST    "\033[0m"

void clear_input()
{
    printf("\x1b[1F"); // Move to beginning of previous line
    printf("\x1b[2K"); // Clear entire line
}

void clear_screen()
{
    printf("\033[1;1H\033[2J");
}

void print_match_end_message(MM_Match *match, Code_t solution, bool show_turns)
{
    if (mm_get_state(match) == MM_MATCH_WON)
    {
        printf("~ ~ You guessed right");
        if (show_turns)
        {
            printf(" in %d turns", mm_get_turns(match));
        }
        if (mm_is_solution_counting_enabled(match) && (mm_get_remaining_solutions(match) > 1))
        {
            printf(" (%d alternative solutions possible)", mm_get_remaining_solutions(match) - 1);
        }
        printf(" ~ ~\n");
    }
    else
    {
        printf("~ ~ Game over - Solution: ");
        print_colors(mm_get_context(match), solution);
        printf(" ~ ~\n");
    }
}

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

bool get_colors_from_string(MM_Context *ctx, const char *string, Code_t *out_code)
{
    if ((int)strlen(string) == mm_get_num_slots(ctx))
    {
        int input_colors[MM_MAX_NUM_SLOTS];
        for (int i = 0; (i < mm_get_num_slots(ctx)); i++)
        {
            input_colors[i] = UINT8_MAX;
            for (int j = 0; j < mm_get_num_colors(ctx); j++)
            {
                if (to_lower(*first_char(colors[j].str)) == to_lower(string[i]))
                {
                    input_colors[i] = j;
                    break;
                }
            }
            if (input_colors[i] == UINT8_MAX)
            {
                return false;
            }
        }
        *out_code = mm_colors_to_code(ctx, input_colors);
        return true;
    }
    else
    {
        return false;
    }
}

bool read_colors(MM_Context *ctx, int turn, Code_t *out_code)
{
    StringBuilder pb = strb_create();
    if (turn != -1)
    {
        strb_append(&pb, "%d/%d colors [", turn, mm_get_max_guesses(ctx));
    }
    else
    {
        strb_append(&pb, "colors [");
    }
    for (int i = 0; i < mm_get_num_colors(ctx); i++)
    {
        strb_append(&pb, "%s%c" RST, colors[i].col, to_lower(*first_char(colors[i].str)));
    }
    strb_append(&pb, "]*%d: ", mm_get_num_slots(ctx));

    bool validated = false;
    char *input    = NULL;

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        clear_input();
        if (input == NULL)
        {
            strb_destroy(&pb);
            return false;
        }
        validated = get_colors_from_string(ctx, input, out_code);
        free(input);
    }
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

void print_guess(int index, MM_Match *match, bool show_hint)
{
    if (index == -1)
    {
        index = mm_get_turns(match) - 1;
    }
    print_colors(mm_get_context(match), mm_get_history_guess(match, index));
    print_feedback(mm_get_context(match), mm_get_history_feedback(match, index));
    if (show_hint && mm_is_solution_counting_enabled(match) && (mm_get_remaining_solutions(match) == 1))
    {
        printf(" *");
    }
}

void print_round_summary_table(MM_Context *ctx,
                               int num_players,
                               char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES],
                               int turns[MAX_NUM_PLAYERS],
                               Code_t guesses[MAX_NUM_PLAYERS][MM_MAX_MAX_GUESSES],
                               int seconds[MAX_NUM_PLAYERS],
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

    Table *tbl = tbl_get_new();
    tbl_set_alternative_style(tbl);
    tbl_set_span(tbl, num_players * 3, 1);
    tbl_override_horizontal_alignment(tbl, TBL_H_ALIGN_CENTER);
    tbl_add_cell_fmt(tbl, "Summary of round %d", round);
    tbl_next_row(tbl);
    tbl_set_hline(tbl, TBL_BORDER_SINGLE);

    for (int i = 0; i < num_players; i++)
    {
        tbl_add_cell_fmt(tbl, " %s ", names[i]);
        tbl_add_cell(tbl, "#B");
        tbl_add_cell(tbl, "#W ");
    }

    tbl_next_row(tbl);
    tbl_set_hline(tbl, TBL_BORDER_SINGLE);

    for (int i = 0; i < max_turns + 1; i++)
    {
        for (int j = 0; j < num_players; j++)
        {
            if (i < turns[j])
            {
                tbl_add_cell_gc(tbl, get_colors_string(ctx, guesses[j][i]));
                int b, w;
                mm_code_to_feedback(ctx, mm_get_feedback(ctx, guesses[j][i], solution), &b, &w);
                tbl_add_cell_fmt(tbl, " %d ", b);
                tbl_add_cell_fmt(tbl, " %d ", w);
            }
            else
            {
                tbl_add_empty_cell(tbl);
                tbl_add_empty_cell(tbl);
                tbl_add_empty_cell(tbl);
            }
        }
        tbl_next_row(tbl);
    }
    tbl_set_hline(tbl, TBL_BORDER_SINGLE);
    for (int i = 0; i < num_players; i++)
    {
        tbl_set_span(tbl, 3, 1);
        if (guesses[i][turns[i] - 1] == solution)
        {
            tbl_add_cell_fmt(tbl, " Time: %d:%02d ", seconds[i] / 60, seconds[i] % 60);
        }
        else
        {
            tbl_add_empty_cell(tbl);
        }
    }
    tbl_next_row(tbl);
    for (int i = 0; i < num_players; i++)
    {
        tbl_set_span(tbl, 3, 1);
        tbl_add_cell_fmt(tbl, " Total points: %d ", points[i]);
    }
    tbl_next_row(tbl);

    for (int i = 1; i < num_players; i++)
    {
        tbl_set_vline(tbl, i * 3, TBL_BORDER_SINGLE);
    }

    tbl_make_boxed(tbl, TBL_BORDER_SINGLE);
    tbl_print(tbl);
    tbl_free(tbl);
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
