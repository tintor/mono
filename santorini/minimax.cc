#include <iostream>

#include "fmt/core.h"
#include "fmt/ostream.h"
#include "core/random.h"

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
