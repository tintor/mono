#include "santorini/execute.h"

using namespace std;

// Board and judge
// ===============

optional<string_view> CanMove(const Board& board, Coord src, Coord dest);

bool IsMoveBlocked(const Board& board) {
    for (Coord a : kAll) {
        if (board(a).figure == board.player) {
            for (Coord b : kAll) {
                if (CanMove(board, a, b) == nullopt) return false;
            }
        }
    }
    return true;
}

optional<string_view> CanNext(const Board& board) {
    if (board.phase == Phase::GameOver) return "bad phase";
    if (board.phase == Phase::PlaceWorker) {
        if (Count(board, L(e.figure == board.player)) != 2) return "need to place worker";
        return nullopt;
    }
    if (!board.moved) return "need to move";
    if (!board.built) return "need to build";
    return nullopt;
}

optional<string_view> Next(Board& board) {
    auto s = CanNext(board);
    if (s != nullopt) return s;
    board.player = Other(board.player);
    board.moved = std::nullopt;
    board.built = false;
    board.artemis_move_src = std::nullopt;
    board.artemis_moves = 0;

    if (board.phase == Phase::PlaceWorker) {
        if (Count(board, L(e.figure != Figure::None)) == 4) board.phase = Phase::MoveBuild;
    } else if (board.phase == Phase::MoveBuild) {
        if (IsMoveBlocked(board)) {
            board.phase = Phase::GameOver;
            board.player = Other(board.player);
        }
    }
    return nullopt;
}

optional<string_view> CanPlace(const Board& board, Coord dest) {
    if (!IsValid(dest)) return "invalid coord";
    if (board.phase != Phase::PlaceWorker) return "bad phase";
    if (board(dest).figure != Figure::None) return "occupied";
    if (Count(board, L(e.figure == board.player)) == 2) return "can't place anymore";
    return nullopt;
}

optional<string_view> Place(Board& board, Coord dest) {
    auto s = CanPlace(board, dest);
    if (s != nullopt) return s;
    board(dest).figure = board.player;
    return nullopt;
}

optional<string_view> CanMove(const Board& board, Coord src, Coord dest) {
    if (!IsValid(src) || !IsValid(dest)) return "invalid coord";
    if (board.phase != Phase::MoveBuild) return "bad phase";

    if (board.my_card() == Card::Artemis) {
        if (board.built) return "Artemis can't move after building";
        if (board.artemis_moves == 2) return "Artemis moved twice already";
        if (board.artemis_moves == 1 && src != *board.moved) return "Artemis can't move both workers";
        if (board.artemis_move_src && dest == *board.artemis_move_src) return "Artemis can't move back to initial position";
    } else {
        if (board.moved) return "moved already";
    }

    if (board(src).figure != board.player) return "player doesn't have figure at src";

    if (board.my_card() == Card::Apollo) {
        if (board(dest).figure != Figure::None && board(dest).figure != Other(board.player)) return "Apollo can't move to square with non-opponent figure";
    } else {
        if (board(dest).figure != Figure::None) return "dest contains another figure";
    }

    if (board(dest).level - board(src).level > 1) return "dest is too high";
    if (!Nearby(src, dest)) return "src and dest aren't nearby";
    return nullopt;
}

// TODO Create a separate Force().
optional<string_view> Move(Board& board, Coord src, Coord dest) {
    auto s = CanMove(board, src, dest);
    if (s != nullopt) return s;
    board(src).figure = board(dest).figure;  // Swap in case of Apollo.
    board(dest).figure = board.player;
    board.moved = dest;

    if (board.my_card() == Card::Artemis) {
        board.artemis_moves += 1;
        if (!board.artemis_move_src) board.artemis_move_src = src;
    }

    if (board(dest).level == 3) board.phase = Phase::GameOver;
    return nullopt;
}

optional<string_view> CanBuild(const Board& board, Coord dest, bool dome) {
    if (!IsValid(dest)) return "invalid coord";
    if (board.phase != Phase::MoveBuild) return "bad phase";
    if (!board.moved) return "need to move";
    if (board.built) return "already built";
    if (board(dest).figure != Figure::None) return "can only build on empty space";
    if (dome && board(dest).level != 3) return "dome can only be built on level 3";
    if (!dome && board(dest).level == 3) return "floor can only be built on levels 0, 1 and 2";
    if (!Nearby(*board.moved, dest)) return "can only build near moved figure";
    return nullopt;
}

optional<string_view> Build(Board& board, Coord dest, bool dome) {
    auto s = CanBuild(board, dest, dome);
    if (s != nullopt) return s;
    if (dome) board(dest).figure = Figure::Dome;
    if (!dome) board(dest).level += 1;
    board.built = true;
    return nullopt;
}

optional<string_view> Execute(Board& board, const Step& step) {
    return std::visit(
        overloaded{[&](NextStep a) { return Next(board); }, [&](PlaceStep a) { return Place(board, a.dest); },
                   [&](MoveStep a) { return Move(board, a.src, a.dest); },
                   [&](BuildStep a) { return Build(board, a.dest, a.dome); }},
        step);
}
