#include <iostream>

#include "fmt/core.h"
#include "fmt/ostream.h"
#include "core/random.h"
#include "core/vector.h"

#include "santorini/reservoir_sampler.h"
#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/greedy.h"

using namespace std;

static std::mt19937_64& Random() {
    static atomic<size_t> seed = 0;
    thread_local bool initialized = false;
    thread_local std::mt19937_64 random;
    if (!initialized) {
        random = std::mt19937_64(seed++);
        initialized = true;
    }
    return random;
}

double MiniMax(Figure player, const Board& board, int depth) {
    if (depth == 0) return ClimbRank(player, board);

    double best_m = (board.player == player) ? -1e100 : 1e100;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        double m = MiniMax(player, new_board, depth - 1);
        if (board.player == player && m > best_m) best_m = m;
        if (board.player != player && m < best_m) best_m = m;
        return true;
    });
    return best_m;
}

// Plan:
// - parallelize it (in MiniMaxValue, if depth = X then make every subcall in
// - show progress during search (ie. number of calls to value())
// - show N best moves with scores
// - use time budget instead of max depth

const double kInfinity = std::numeric_limits<double>::infinity();

// Return value of <initial_state>, from the perspective of <player>.
double MiniMaxValue(const Figure player, const Board& initial_board, const int depth, const bool maximize, double alpha, double beta) {
    if (depth == 0 || initial_board.phase == Phase::GameOver) return ClimbRank(player, initial_board);


    std::vector<std::pair<double, Board>> boards;
    AllValidActions(initial_board, [&boards, player](Action& action, const Board& board) {
        boards << std::pair{ClimbRank(player, board), board};
        return true;
    });
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
        const double m = (depth == 1 || board.phase == Phase::GameOver) ? value : MiniMaxValue(player, board, depth - 1, !maximize, alpha, beta);
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

#if 0
// Return action with the highest value (also return the value), from the perspective of <next_player>.
Action AutoMiniMax(const Board& initial_board, const int depth) {
    Action best_action;
    double best_score = -kInfinity;
    double alpha = -kInfinity;
    double beta = kInfinity;
    AllValidActions(initial_board, [&](Action& action, const Board& board) {
        double m = MiniMaxValue(state, depth, /*maximize*/false, alpha, beta);
        if (m > best_score) {
            best_action = action;
            best_score = m;
        }
        return true;
    });
    return best_action;
}
#endif

Step AutoMiniMax(const Board& board, const int depth) {
    Step best_step;

    size_t count = 0;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        best_step = step;
        count += 1;
        return count < 2;
    });
    if (count == 1) return best_step;

    auto& random = Random();
    double best_score = -1e100;
    ReservoirSampler sampler;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        double m = MiniMax(board.player, new_board, depth);
        if (m == best_score && sampler(random)) best_step = step;
        if (m > best_score) {
            count = 1;
            best_step = step;
            best_score = m;
        }
        return true;
    });
    return best_step;
}
