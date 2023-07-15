#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include "../multiplayer/protocol.h"

bool begins_with(const char *prefix, const char *str);
size_t str_split(char *str, char **out_strs, size_t num_delimiters, ...);
size_t get_line_of_string(const char *string, size_t line_index, char **out_start);
char *skip_ansi(const char *str);
char *strip(char *str);
const char *first_char(const char *string);
char to_lower(char c);
char to_upper(char c);
const char *plstate_to_str(PlayerState state);
