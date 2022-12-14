#pragma once
#include "../mastermind.h"

#define MAX_NUM_PLAYERS       4
#define MAX_PLAYER_NAME_BYTES 11 // Including \0
#define MAX_NUM_ROUNDS        10

typedef struct
{
    char name[MAX_PLAYER_NAME_BYTES];
} NicknamePackage_Q;

typedef struct
{
    bool is_accepted;
    bool waiting_for_others;
} NicknamePackage_R;

typedef struct
{
    int num_rounds;
    int max_guesses;
    int num_slots;
    int num_players;
    char players[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES];
    int num_colors;
} GameBeginPackage_R;

typedef struct
{
    int dummy;
} ReadyPackage_Q;

typedef struct
{
    bool waiting_for_others;
} ReadyPackage_R;

typedef struct
{
    int dummy;
} RoundBeginPackage_R;

typedef struct
{
    Code_t guess;
} GuessPackage_Q;

typedef struct
{
    Feedback_t feedback;
    bool lost;
    Code_t solution;
    bool waiting_for_others;
} FeedbackPackage_R;

typedef struct
{
    int points[MAX_NUM_PLAYERS];
    int num_turns[MAX_NUM_PLAYERS];
    Feedback_t feedbacks[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
    Code_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
} RoundEndPackage_R;
