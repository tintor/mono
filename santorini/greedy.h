#pragma once
#include "santorini/board.h"
#include "santorini/action.h"

// Random action.
Action AutoRandom(const Board& board);

// Winning action OR a random action.
Action AutoGreedy(const Board& board);

struct Weights {
    double level1 = 1;
    double level2 = 10;
    double level3 = 10;
    double reachable_cell = 0;
    double mass1 = 0;
    double mass2 = 0;
    double mass3 = 0;
};

// Winning action OR an action with the highest value (linear combination of features).
double ClimbRank(Figure player, const Board& board, const Weights& weights = Weights());
Action AutoClimber(const Board& board, const Weights& weights = Weights());
