#pragma once
#include "santorini/execute.h"
#include "absl/container/flat_hash_set.h"

template <typename Visitor>
bool Visit(const Board& board, const Step& step, const Visitor& visit) {
    Board my_board = board;
    if (Execute(my_board, step) != std::nullopt) return true;
    return visit(my_board, step);
}

#define VISIT(A) \
    if (!Visit(board, A, visit)) return false;

template <typename Visitor>
bool AllValidSteps(const Board& board, const Visitor& visit) {
    if (board.phase == Phase::PlaceWorker) {
        VISIT(NextStep{});
        for (Coord e : kAll) VISIT(PlaceStep{e});
        return true;
    }

    // Faster logic when playing without cards.
    if (board.card1 == Card::None && board.card2 == Card::None) {
        if (!board.moved) {
            // Generate moves
            for (Coord e : kAll) {
                if (board(e).figure == board.player) {
                    // TODO Only iterate nearby e!
                    for (Coord d : kAll)
                        if (d != e && Nearby(e, d) && board(d).figure == Figure::None) VISIT((MoveStep{e, d}));
                }
            }
            return true;
        }
        if (!board.built) {
            // Generate builds
            // TODO Only iterate nearby board.moved.value()!
            for (Coord d : kAll) if (d != *board.moved && Nearby(*board.moved, d) && board(d).figure == Figure::None) {
                VISIT((BuildStep{d, board(d).level == 3}));
            }
        }
        VISIT(NextStep{});
        return true;
    }

    // Generic case
    VISIT(NextStep{});
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

// Can return duplicate boards.
template <typename Visitor>
bool _AllValidActions(const Board& board, const Visitor& visit, Action* temp = nullptr) {
    Action _temp;
    if (!temp) temp = &_temp;

    return AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        temp->push_back(step);
        if (std::holds_alternative<NextStep>(step) || new_board.phase == Phase::GameOver) {
            if (!visit(*temp, new_board)) return false;
        } else {
            if (!_AllValidActions(new_board, visit, temp)) return false;
        }
        temp->pop_back();
        return true;
    });
}

// Deduplicates boards (if needed).
template <typename Visitor>
bool AllValidBoards(const Board& board, const Visitor& visit) {
    if (DeduplicateBoards(board.card1) || DeduplicateBoards(board.card2)) {
        // Return only unique boards.
        absl::flat_hash_set<MiniBoard> boards;
        return _AllValidActions(board, [&visit, &boards](Action& action, const Board& new_board) {
            return !boards.emplace(new_board).second || visit(action, new_board);
        });
    }
    return _AllValidActions(board, visit);
}
