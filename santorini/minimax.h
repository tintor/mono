#pragma once
#include <limits>
#include <vector>
#include <future>

// Plan:
// - separate minimax algorithm from santorini
// - parallelize it (in MiniMaxValue, if depth = X then make every subcall in
// - show progress during search (ie. number of calls to value())
// - show N best moves with scores
// - use time budget instead of max depth

const double kInfinity = std::numeric_limits<double>::infinity();

template<typename Player, typename State, typename Action, typename IsTerminalFn, typename StateEnumeratorFn, typename ValueFn>
struct MiniMaxCommon {
    const IsTerminalFn& is_terminal;
    const StateEnumeratorFn& state_enumerator;
    const ValueFn& value;
    Player player;

    // Return value of <initial_state>, from the perspective of <player>.
    double TreeValue(const State& initial_state, const int depth, const bool maximize, double alpha, double beta) {
        if (depth == 0 || is_terminal(initial_state)) return value(player, initial_state);

        // TODO In Santorini, if no action is possible for maximizing player that is considered defeat. It is accidental here that -inf will be returned in that case.
        double best_m = maximize ? -kInfinity : kInfinity;
        std::vector<State> states = state_enumerator(initial_state);
        // TODO sort states by value (alpha-beta will take care of short-circuiting)

        for (const State& state : states) {
            const double m = TreeValue(state, depth - 1, !maximize, alpha, beta);
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
};

// Return action with the highest value (also return the value), from the perspective of <next_player>.
template<typename Player, typename State, typename Action, typename IsTerminalFn, typename StateEnumeratorFn, typename StateActionEnumeratorFn, typename ValueFn>
std::pair<Action, double> MiniMax(
        const Player& next_player,
        const State& initial_state,
        const int depth,
        const IsTerminalFn& is_terminal,
        const StateEnumeratorFn& state_enumerator,
        const StateActionEnumeratorFn& state_action_enumerator,
        const ValueFn& value) {
    Action best_action;
    double best_score = -kInfinity;
    MiniMaxCommon common{is_terminal, state_enumerator, value, next_player};
    state_action_enumerator(initial_state, [&](const State& state, const Action& action) {
        double m = common.TreeValue(state, depth, /*maximize*/false, -kInfinity, kInfinity);
        if (m > best_score) {
            best_action = action;
            best_score = m;
        }
        return true;
    });
    return {best_action, best_score};
}
