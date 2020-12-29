#pragma once
#include "santorini/board.h"
#include "santorini/action.h"
#include "santorini/policy.h"

Action AutoMCTS(const Board& board, const size_t iterations, bool climber2 = false);

inline Policy MCTS(const size_t iterations, bool climber2 = false) {
    return QuickStart([=](const Board& board) {
        return AutoMCTS(board, iterations, climber2);
    });
}
