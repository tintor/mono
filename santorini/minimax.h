#pragma once
#include "santorini/action.h"
#include "santorini/board.h"
#include "santorini/policy.h"

Action AutoMiniMax(const Board& board, const int depth, bool climber2 = false, bool extra = false);

inline Policy MiniMax(const int depth, bool climber2 = false, bool extra = false) {
    return QuickStart([=](const Board& board) {
        return AutoMiniMax(board, depth, climber2, extra);
    });
}
