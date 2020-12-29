#pragma once
#include <functional>
#include "santorini/board.h"
#include "santorini/action.h"
#include "santorini/random.h"

using Policy = std::function<Action(const Board&)>;

inline Policy QuickStart(const Policy& policy) {
    return [=](const Board& board) {
        if (board.phase == Phase::PlaceWorker && IsEmpty(board)) {
            std::vector<Action> actions;
            for (Coord a : kInterior)
                for (Coord b : kInterior) if (a < b) if (!Nearby(a, b) || a == kCenter || b == kCenter) {
                    actions.push_back({PlaceStep{a}, PlaceStep{b}, NextStep{}});
                }
            auto& random = Random();
            return actions[RandomInt(actions.size(), random)];
        }
        return policy(board);
    };
}
