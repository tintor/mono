#pragma once
#include <random>
#include <variant>
#include <iostream>

#include "santorini/execute.h"

using namespace std;

template <typename Visitor>
bool Visit(const Board& board, const Step& step, const Visitor& visit) {
    Board my_board = board;
    if (Execute(my_board, step) != nullopt) return true;
    return visit(my_board, step);
}

#define VISIT(A) \
    if (!Visit(board, A, visit)) return false;

template <typename Visitor>
bool AllValidSteps(const Board& board, const Visitor& visit) {
    VISIT(NextStep{});
    if (board.phase == Phase::PlaceWorker) {
        for (Coord e : kAll) VISIT(PlaceStep{e});
        return true;
    }
    for (Coord e : kAll) {
        if (board(e).figure == board.player) {
            for (Coord d : kAll)
                if (d != e) VISIT((MoveStep{e, d}));
        }
    }
    for (bool dome : {false, true})
        for (Coord e : kAll) VISIT((BuildStep{e, dome}));
    return true;
}

template <typename Visitor>
bool AllValidStepSequences(const Board& board, Action& temp, const Visitor& visit) {
    return AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        temp.push_back(step);
        if (std::holds_alternative<NextStep>(step) || new_board.phase == Phase::GameOver) {
            if (!visit(temp, new_board)) return false;
        } else {
            if (!AllValidStepSequences(new_board, temp, visit)) return false;
        }
        temp.pop_back();
        return true;
    });
}
