#include "santorini/mcts.h"

#include <vector>
#include "core/random.h"

#include "santorini/enumerator.h"
#include "santorini/reservoir_sampler.h"
#include "santorini/greedy.h"
#include "santorini/execute.h"

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

inline int RandomInt(int count, std::mt19937_64& random) { return std::uniform_int_distribution<int>(0, count - 1)(random); }

// Rollout board using AutoGreedy!
static size_t Rollout(Figure player, Board board) {
    Action action;
    while (true) {
        action = AutoGreedy(board);
        for (const Step& step : action) {
            Check(Execute(board, step) == nullopt);
            if (board.phase == Phase::GameOver) return (board.player == player) ? 1 : 0;
        }
    }
}

struct Node {
    Action action;
    Board board;  // board state post-action
    size_t w = 0;  // number of wins
    size_t n = 0;  // total number of rollouts (w/n is win ratio)
    vector<std::unique_ptr<Node>> children;
};

static size_t ChooseChild(size_t N, const vector<std::unique_ptr<Node>>& children) {
    size_t best_i = 0;
    double best_ucb1 = 0;
    for (size_t i = 0; i < children.size(); i++) {
        const auto& child = children[i];
        double ucb1 = (child->n == 0) ? std::numeric_limits<double>::infinity()
                                      : (child->w / child->n + 2 * sqrt(log(N) / child->n));
        if (ucb1 > best_ucb1) {
            best_ucb1 = ucb1;
            best_i = i;
        }
    }
    return best_i;
}

static void Expand(const Board& board, vector<std::unique_ptr<Node>>& out) {
    AllValidActions(board, [&](Action& action, const Board& new_board) {
        auto node = std::make_unique<Node>();
        node->action = action;
        node->board = new_board;
        out.push_back(std::move(node));
        return true;
    });
}

static size_t MCTS_Iteration(size_t N, Figure player, std::unique_ptr<Node>& node) {
    if (node->board.phase == Phase::GameOver) {
        size_t e = (player == node->board.player) ? 1 : 0;
        node->w += e;
        node->n += 1;
        return e;
    }

    if (node->children.size() > 0) {
        size_t i = ChooseChild(N, node->children);
        size_t e = MCTS_Iteration(N, player, node->children[i]);
        node->w += e;
        node->n += 1;
        return e;
    }

    if (node->n == 0) {
        size_t e = Rollout(player, node->board);
        node->w += e;
        node->n += 1;
        return e;
    }

    Expand(node->board, node->children);
    Check(node->children.size() > 0);

    auto& child = node->children[RandomInt(node->children.size(), Random())];
    size_t e = MCTS_Iteration(N, player, child);
    node->w += e;
    node->n += 1;
    return e;
}

static optional<Action> WinAction(const Board& board) {
    optional<Action> win_action;
    AllValidActions(board, [&](const Action& action, const Board& new_board) {
        if (new_board.phase == Phase::GameOver && new_board.player == board.player) {
            win_action = action;
            return false;
        }
        return true;
    });
    return win_action;
}

Action AutoMCTS(const Board& board, const size_t iterations) {
    auto wa = WinAction(board);
    if (wa.has_value()) return wa.value();

    vector<std::unique_ptr<Node>> children;
    Expand(board, children);
    if (children.size() == 1) return children[0]->action;

    for (size_t i = 0; i < iterations; i++) {
        size_t ci = ChooseChild(i, children);
        MCTS_Iteration(i, board.player, children[ci]);
        //if ((i + 1) % 100 == 0) print("{} mcts iterations\n", i + 1);
    }

    double best_v = 0;
    size_t best_i = 0;
    for (size_t i = 0; i < children.size(); i++) {
        double v = double(children[i]->w) / children[i]->n;
        if (v > best_v) {
            best_v = v;
            best_i = i;
        }
    }
    return children[best_i]->action;
}
