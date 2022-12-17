#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <readline/readline.h>
#include <inttypes.h>
#include <errno.h>

#include "console.h"
#include "multiplayer/protocol.h"
#include "string_builder.h"
#include "string_util.h"
#include "table.h"

#define BLK    "\033[38:2:000:000:000m"
#define WHT    "\033[38:2:255:255:255m"
#define BLK_BG "\033[48:2:000:000:000m"
#define WHT_BG "\033[48:2:255:255:255m"
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

void any_key()
{
    printf("Press enter to continue...");
    getchar();
}

void print_winning_message(int num_turns)
{
    printf("~ ~ Game won in %d steps ~ ~\n\n", num_turns);
}

void print_losing_message(int num_max_guesses)
{
    printf("~ ~ Game over - You could not make it in %d turns ~ ~\n", num_max_guesses);
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

Code_t read_colors(MM_Context *ctx, int turn)
{
    StringBuilder pb = strb_create();
    strb_append(&pb, "%d/%d colors [", turn + 1, mm_get_max_guesses(ctx));
    for (int i = 0; i < mm_get_num_colors(ctx); i++)
    {
        strb_append(&pb, "%c", mm_get_color_char(ctx, i));
    }
    strb_append(&pb, "]*%d: ", mm_get_num_slots(ctx));

    bool validated = false;
    char *input    = NULL;
    int colors[MAX_NUM_COLORS];

    while (!validated)
    {
        input = readline(strb_to_str(&pb));
        if (input == NULL)
        {
            continue;
        }
        clear_input();

        if ((int)strlen(input) == mm_get_num_slots(ctx))
        {
            validated = true;
            for (int i = 0; (i < mm_get_num_slots(ctx)) && validated; i++)
            {
                colors[i] = UINT8_MAX;
                for (int j = 0; j < mm_get_num_colors(ctx); j++)
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
    Code_t result = mm_colors_to_code(ctx, colors);
    print_colors(ctx, result);
    printf("\n");
    strb_destroy(&pb);
    return result;
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
        strb_append(&builder, "%s  ", mm_get_color(ctx, mm_get_color_at_pos(mm_get_num_colors(ctx), input, i)));
    }
    return strb_to_str(&builder);
}

void print_feedback(MM_Context *ctx, Feedback_t feedback)
{
    int b, w;
    mm_code_to_feedback(ctx, feedback, &b, &w);
    printf(BLK_BG WHT " Black: %d " RST "    " WHT_BG BLK " White: %d " RST, b, w); // 24 chars
}

void print_game_summary_table(int num_players,
                              char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES],
                              int total_points[MAX_NUM_PLAYERS],
                              int best_points)
{
    Table *tbl = get_empty_table();
    set_span(tbl, num_players, 1);
    override_horizontal_alignment(tbl, H_ALIGN_CENTER);
    add_cell(tbl, " Summary of game ");
    next_row(tbl);
    set_hline(tbl, BORDER_SINGLE);

    for (int i = 0; i < num_players; i++)
    {
        override_horizontal_alignment(tbl, H_ALIGN_CENTER);
        add_cell_fmt(tbl, " %s ", names[i]);
    }

    next_row(tbl);
    set_hline(tbl, BORDER_SINGLE);

    for (int j = 0; j < num_players; j++)
    {
        add_cell_fmt(tbl, " %d points ", total_points[j]);
    }

    next_row(tbl);
    set_hline(tbl, BORDER_SINGLE);

    for (int j = 0; j < num_players; j++)
    {
        override_horizontal_alignment(tbl, H_ALIGN_CENTER);
        if (total_points[j] == best_points)
        {
            add_cell(tbl, " WINNER! ");
        }
        else
        {
            add_empty_cell(tbl);
        }
    }

    next_row(tbl);
    set_all_vlines(tbl, BORDER_SINGLE);
    make_boxed(tbl, BORDER_SINGLE);
    print_table(tbl);
    free_table(tbl);
}

void print_round_summary_table(MM_Context *ctx,
                               int num_players,
                               char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES],
                               int turns[MAX_NUM_PLAYERS],
                               Code_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES],
                               Feedback_t feedbacks[MAX_NUM_PLAYERS][MAX_MAX_GUESSES],
                               int round)
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
    add_cell_fmt(tbl, "Summary of round %d", round + 1);
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
                mm_code_to_feedback(ctx, feedbacks[j][i], &b, &w);
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
        add_cell_fmt(tbl, " %d turns", turns[i]);
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
        if (input == NULL)
        {
            return false;
        }
        clear_input();
        if (input[0] == '\0')
        {
            free(input);
            return default_value;
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
