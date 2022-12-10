#pragma once
#include <stdint.h>
#include "mastermind.h"
#include "../multiplayer/protocol.h"

void clear_input();
void clear_screen();
void any_key();
uint8_t digit_to_uint(char c);
void print_winning_message(uint8_t num_turns);
void print_losing_message(uint8_t num_max_guesses);

uint8_t read_feedback(MM_Context *ctx);
uint16_t read_colors(MM_Context *ctx, uint8_t turn);
void print_colors(MM_Context *ctx, uint16_t input);
void print_feedback(MM_Context *ctx, uint8_t feedback);
char *get_colors_string(MM_Context *ctx, uint16_t input);

void print_game_summary_table(int num_players,
    char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_LENGTH],
    int total_points[MAX_NUM_PLAYERS],
    int best_points);
void print_round_summary_table(MM_Context *ctx,
    int num_players,
    char names[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_LENGTH],
    int turns[MAX_NUM_PLAYERS],
    uint16_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES],
    uint8_t feedbacks[MAX_NUM_PLAYERS][MAX_MAX_GUESSES],
    int round);
