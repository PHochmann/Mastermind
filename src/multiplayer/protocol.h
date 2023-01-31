#pragma once
#include "../mastermind.h"

// #define DEBUG_TRANSITION_LOG
#define MAX_NUM_PLAYERS       4
#define MAX_PLAYER_NAME_BYTES 31 // Including \0
#define MAX_NUM_ROUNDS        10

typedef enum
{
    PLAYER_STATE_NONE,
    PLAYER_STATE_CONNECTED,
    PLAYER_STATE_RULES_RECEIVED,
    PLAYER_STATE_CHOOSING_NAME,
    PLAYER_STATE_SENT_NAME,
    PLAYER_STATE_NOT_ACKED,
    PLAYER_STATE_ACKED,
    PLAYER_STATE_GUESSING,
    PLAYER_STATE_AWAITING_FEEDBACK,
    PLAYER_STATE_GOT_FEEDBACK,
    PLAYER_STATE_FINISHED,
    PLAYER_STATE_ABORTED,
    PLAYER_STATE_DISCONNECTED,
} PlayerState;

typedef struct
{
    PlayerState state;
} StateTransition_RQ;

typedef struct server
{
    PlayerState from;
    PlayerState to;
} Transition;

// Is sent by server directly after connection
typedef struct
{
    int player_id;
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
    MM_MatchState match_state;
    Code_t solution;
    bool waiting_for_others;
} FeedbackPackage_R;

typedef struct
{
    int winner_pl;
    bool win_reason_quicker;
    int points[MAX_NUM_PLAYERS];
    int num_turns[MAX_NUM_PLAYERS];
    int seconds[MAX_NUM_PLAYERS];
    Code_t guesses[MAX_NUM_PLAYERS][MAX_MAX_GUESSES];
    Code_t solution;
} RoundEndPackage_R;
