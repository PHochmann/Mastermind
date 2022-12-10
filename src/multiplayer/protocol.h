#pragma once
#include <stdint.h>
#include "../mastermind.h"

#define MAX_NUM_PLAYERS          2
#define MAX_PLAYER_NAME_LENGTH  10
#define MAX_COLOR_LENGTH        20
#define MAX_NUM_ROUNDS          10

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
    int num_rounds;
    int max_guesses;
    int num_slots;
    int num_players;
    char players[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_LENGTH];
    int num_colors;
    char colors[MAX_NUM_COLORS][MAX_COLOR_LENGTH];
} GameBeginPackage_R;

typedef struct
{
    uint16_t guess;
} GuessPackage_Q;

typedef struct
{
    uint8_t feedback;
    bool lost;
    uint16_t solution;
} FeedbackPackage_R;

typedef struct
{
    uint8_t dummy;
} RoundBeginPackage_R;


typedef struct
{
    int points[MAX_NUM_PLAYERS];
    int num_turns[MAX_NUM_PLAYERS];
    uint8_t feedbacks[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
    uint16_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
} RoundEndPackage_R;

typedef struct
{
    uint8_t dummy;
} ReadyPackage_Q;
