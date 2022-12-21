#pragma once
#include "../mastermind.h"

#define MAX_NUM_PLAYERS       4
#define MAX_PLAYER_NAME_BYTES 11 // Including \0
#define MAX_NUM_ROUNDS        10

typedef enum
{
    PLAYER_STATE_NOT_CONNECTED = 0,
    PLAYER_STATE_CONNECTED,
    PLAYER_STATE_NAME_PUTTING,
    PLAYER_STATE_NAME_SENT,
    PLAYER_STATE_NOT_ACKED,
    PLAYER_STATE_ACKED,
    PLAYER_STATE_GUESSING,
    PLAYER_STATE_AWAITING_FEEDBACK,
    PLAYER_STATE_FINISHED,
    PLAYER_STATE_ABORTED,
    PLAYER_STATE_TIMEOUT
} PlayerState;

typedef struct
{
    PlayerState new_state;
} PlayerStateTransition_Q;

typedef struct
{
    PlayerState new_state;
    bool waiting_for_others;
} PlayerStateTransition_R;

// Is sent by server directly after connection
typedef struct
{
    int num_rounds;
    int max_guesses;
    int num_slots;
    int num_players;
    int num_colors;
} RulesPackage_R;

// Is sent by client to propose a nickname
typedef struct
{
    char name[MAX_PLAYER_NAME_BYTES];
} NicknamePackage_Q;

// Is sent by server to announce all player nicknames after each player's nickname got accepted
typedef struct
{
    char players[MAX_NUM_PLAYERS][MAX_PLAYER_NAME_BYTES];
} AllNicknamesPackage_R;

typedef struct
{
    Code_t guess;
} GuessPackage_Q;

typedef struct
{
    Feedback_t feedback;
    Code_t solution;
} FeedbackPackage_R;

typedef struct
{
    int points[MAX_NUM_PLAYERS];
    int num_turns[MAX_NUM_PLAYERS];
    Feedback_t feedbacks[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
    Code_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
} RoundEndPackage_R;
