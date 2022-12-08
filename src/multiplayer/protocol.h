#pragma once
#include <stdint.h>
#include "../mastermind.h"

#define MAX_NUM_PLAYERS          2
#define MAX_PLAYER_NAME_LENGTH  10
#define MAX_COLOR_LENGTH        20

typedef struct
{
    char name[MAX_PLAYER_NAME_LENGTH];
} NicknamePackage_Q;

typedef struct
{
    bool is_accepted;
} NicknamePackage_R;

typedef struct
{
    uint8_t num_players;
    char players[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_LENGTH];
    uint8_t max_guesses;
    uint8_t num_slots;
    uint8_t num_colors;
    char colors[MAX_NUM_COLORS][MAX_COLOR_LENGTH];
    uint8_t num_rounds;
} GameBeginPackage_R;

typedef struct
{
    uint16_t guess;
} GuessPackage_Q;

typedef struct
{
    uint8_t feedback;
    uint16_t solution;
} FeedbackPackage_R;

typedef struct
{
    uint8_t num_turns[MAX_NUM_PLAYERS];
    uint8_t feedbacks[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
    uint16_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
} RoundEndPackage_R;
