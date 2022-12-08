#pragma once
#include "mastermind.h"

void play_singleplayer(MM_Context *ctx);
void play_recommender(MM_Context *ctx, MM_Strategy strategy);
void play_duel(MM_Context *ctx, uint8_t num_rounds);
