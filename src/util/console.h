#pragma once
#include <stdint.h>
#include "mastermind.h"

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
