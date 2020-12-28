#pragma once
#include "santorini/board.h"
#include "santorini/action.h"

// Random action.
Action AutoRandom(const Board& board);

// Winning action OR a random action.
Action AutoGreedy(const Board& board);

// Winning action OR an action with the highest value (linear combination of features).
double ClimbRank(Figure player, const Board& board);
Action AutoClimber(const Board& board);
