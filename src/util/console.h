#pragma once
#include "../multiplayer/protocol.h"
#include "../mastermind.h"
#include <stdint.h>

char *readline_fmt(const char *fmt, ...);
void clear_input();
void clear_screen();
void await_enter();
void print_match_end_message(MM_Match *match, Code_t solution, bool show_turns);

bool get_colors_from_string(MM_Context *ctx, const char *string, Code_t *out_code);
bool read_colors(MM_Context *ctx, int turn, Code_t *out_code);
void print_colors(MM_Context *ctx, Code_t input);
void print_feedback(MM_Context *ctx, Feedback_t feedback);
void print_guess(int index, MM_Match *match, bool show_hint);
char *get_colors_string(MM_Context *ctx, Code_t input);

void print_round_summary_table(MM_Context *ctx,
                               int num_players,
                               char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES],
                               int turns[MAX_NUM_PLAYERS],
                               Code_t guesses[MAX_NUM_PLAYERS][MM_MAX_MAX_GUESSES],
                               int seconds[MAX_NUM_PLAYERS],
                               Code_t solution,
                               int round,
                               int points[MAX_NUM_PLAYERS]);

bool readline_int(const char *prompt, int default_value, int min, int max, int *result);
