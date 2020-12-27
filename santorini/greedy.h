#pragma once
#include "santorini/board.h"
#include "santorini/action.h"

Step AutoRandom(const Board& board);
Step AutoGreedy(const Board& board);

double ClimbRank(Figure player, const Board& board);
Step AutoClimber(const Board& board);
