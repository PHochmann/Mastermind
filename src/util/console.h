#pragma once
#include "../multiplayer/protocol.h"
#include "../mastermind.h"
#include <stdint.h>

char *readline_fmt(const char *fmt, ...);
void clear_input();
void clear_screen();
void await_enter();
void print_winning_message(int num_turns);
void print_losing_message(MM_Context *ctx, Code_t solution);

Feedback_t read_feedback(MM_Context *ctx);
bool read_colors(MM_Context *ctx, int turn, Code_t *out_code);
void print_colors(MM_Context *ctx, Code_t input);
void print_feedback(MM_Context *ctx, Feedback_t feedback);
char *get_colors_string(MM_Context *ctx, Code_t input);

void print_round_summary_table(MM_Context *ctx,
                               int num_players,
                               char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES],
                               int turns[MAX_NUM_PLAYERS],
                               Code_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES],
                               Code_t solution,
                               int round,
                               int points[MAX_NUM_PLAYERS]);

bool readline_int(const char *prompt, int default_value, int min, int max, int *result);
