#include <iostream>

#include "fmt/core.h"
#include "fmt/ostream.h"
#include "core/random.h"
#include "core/vector.h"

#include "santorini/reservoir_sampler.h"
#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/greedy.h"
#include "santorini/random.h"

using namespace std;

// Plan:
// - parallelize it (in MiniMaxValue, if depth = X then make every subcall in
// - show progress during search (ie. number of calls to value())
// - show N best moves with scores
// - use time budget instead of max depth
// - search in background (while human is thinking)

const double kInfinity = numeric_limits<double>::infinity();

// Return value of <initial_board>, from the perspective of <player>.
static double MiniMaxValue(const Figure player, const Board& initial_board, const int depth, const bool maximize, double alpha, double beta, const bool climber2) {
    Weights weights;
    if (climber2) weights = Weights{.mass1 = 0.2, .mass2 = 0.4, .mass3 = 0.8};

    if (depth == 0 || initial_board.phase == Phase::GameOver) return ClimbRank(player, initial_board, weights);


    vector<pair<double, Board>> boards;
    AllValidBoards(initial_board, [&boards, player, &weights](Action& action, const Board& board) {
        boards << pair{ClimbRank(player, board, weights), board};
        return true;
    });
    // TODO Compare MiniMax with and without this sorting!
    // Sort boards by value (alpha-beta will take care of short-circuiting).
    if (maximize) {
        sort(boards, [](const auto& a, const auto& b) { return a.first > b.first; });
    } else {
        sort(boards, [](const auto& a, const auto& b) { return a.first < b.first; });
    }
    // Remove duplicates.
    boards.erase(std::unique(boards.begin(), boards.end()), boards.end());

    // TODO In Santorini, if no action is possible for maximizing player that is considered defeat. It is accidental here that -inf will be returned in that case.
    double best_m = maximize ? -kInfinity : kInfinity;
    for (const auto& [value, board] : boards) {
        const double m = (depth == 1 || board.phase == Phase::GameOver) ? value : MiniMaxValue(player, board, depth - 1, !maximize, alpha, beta, climber2);
        if (maximize) {
            if (m > best_m) best_m = m;
            if (best_m > alpha) alpha = best_m;
            if (beta <= alpha) break;
        } else {
            if (m < best_m) best_m = m;
            if (best_m < beta) beta = best_m;
            if (beta <= alpha) break;
        }
    }
    return best_m;
}

Action AutoMiniMax(const Board& initial_board, const int depth, bool climber2, bool extra) {
    Action best_action;

    size_t count = 0;
    AllValidBoards(initial_board, [&](const Action& action, const Board& board) {
        best_action = action;
        count += 1;
        return count < 2;
    });
    if (count == 1) return best_action;

    auto& random = Random();
    double best_m = -kInfinity;
    double alpha = -kInfinity;
    double beta = kInfinity;
    ReservoirSampler sampler;
    AllValidBoards(initial_board, [&](const Action& action, const Board& board) {
        double m = MiniMaxValue(initial_board.player, board, depth, /*maximize*/false, alpha, beta, climber2);
        if (extra && m == kInfinity) {
            best_m = m;
            best_action = action;
            return false;
        }
        if (m == best_m) {
            if (sampler(random)) best_action = action;
        } else if (m > best_m) {
            sampler.count = 1;
            best_action = action;
            best_m = m;
        }
        return true;
    });
    return best_action;
}
