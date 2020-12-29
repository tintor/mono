#include "santorini/greedy.h"

#include "fmt/core.h"

#include "santorini/random.h"
#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/reservoir_sampler.h"

using namespace std;

Coord MyRandomFigure(const Board& board, std::mt19937_64& random) {
    Coord out;
    ReservoirSampler sampler;
    for (Coord e : kAll)
        if (board(e).figure == board.player && sampler(random)) out = e;
    return out;
}

Step RandomStep(const Board& board) {
    std::mt19937_64& random = Random();
    if (board.phase == Phase::PlaceWorker) {
        int c = RandomInt(1 + 8, random);
        if (c == 0) return NextStep{};
        return PlaceStep{Coord::Random(random)};
    }

    int c = RandomInt(1 + 2 * 4, random);
    if (c == 0) return NextStep{};
    if (c <= 4) return MoveStep{MyRandomFigure(board, random), Coord::Random(random)};
    return BuildStep{Coord::Random(random), bool(RandomInt(2, random))};
}

bool IsValid(const Board& board, const Step& step) {
    Board my_board = board;
    return Execute(my_board, step) == nullopt;
}

Action AutoRandom(const Board& board) {
    auto& random = Random();
    ReservoirSampler sampler;
    Action choice;
    AllValidActions(board, [&](const Action& action, const Board& new_board) {
        if (sampler(random)) choice = action;
        return true;
    });
    return choice;
}

Action AutoGreedy(const Board& board) {
    auto& random = Random();
    Action choice;
    Action loose_choice;
    ReservoirSampler sampler;
    AllValidActions(board, [&](const Action& action, const Board& new_board) {
        if (new_board.phase == Phase::GameOver && new_board.player == board.player) {
            choice = action;
            sampler.count = 1;
            return false;
        }
        if (new_board.phase != Phase::GameOver) {
            if (sampler(random)) choice = action;
        } else {
            loose_choice = action;
        }
        return true;
    });
    return choice.empty() ? loose_choice : choice;
}

const double kInfinity = std::numeric_limits<double>::infinity();

double ClimbRank(Figure player, const Board& board, const Weights& weights) {
    if (board.phase == Phase::GameOver) return (player == board.player) ? kInfinity : -kInfinity;

    double rank = 0;
    const Figure other = Other(player);
    bool mass = weights.mass1 != 0 || weights.mass2 != 0 || weights.mass3 != 0;
    for (Coord e : kAll) {
        const auto level = board(e).level;

        if (board(e).figure == player) {
            if (level == 1) rank += weights.level1;
            if (level == 2) rank += weights.level2;
            if (level == 3) rank += weights.level3;

            if (mass)
            for (Coord m : kAll) if (m != e && Nearby(e, m) && board(m).figure == Figure::None) {
                if (board(m).level == 1) rank += weights.mass1;
                if (board(m).level == 2) rank += weights.mass2;
                if (board(m).level == 3) rank += weights.mass3;
            }
        }
        if (board(e).figure == other) {
            if (level == 1) rank -= weights.level1;
            if (level == 2) rank -= weights.level2;
            if (level == 3) rank -= weights.level3;

            if (mass)
            for (Coord m : kAll) if (m != e && Nearby(e, m) && board(m).figure == Figure::None) {
                if (board(m).level == 1) rank -= weights.mass1;
                if (board(m).level == 2) rank -= weights.mass2;
                if (board(m).level == 3) rank -= weights.mass3;
            }
        }
    }

    if (weights.reachable_cell != 0) {
        for (Coord e : kAll) {
            Figure f = board(e).figure;
            if (f == player || f == other) {
                Board b;
                b.player = f;
                size_t reachable = 0;
                for (Coord j : kAll) if (j != e && board(j).figure == Figure::None && CanMove(b, e, j)) {
                    reachable += 1;
                }
                if (f == player) rank += weights.reachable_cell * reachable;
                if (f == other) rank -= weights.reachable_cell * reachable;
            }
        }
    }
    return rank;
}

Action AutoClimber(const Board& board, const Weights& weights) {
    auto& random = Random();
    Action choice;
    ReservoirSampler sampler;
    double best_rank = -kInfinity;
    AllValidActions(board, [&](const Action& action, const Board& new_board) {
        if (new_board.phase == Phase::GameOver) {
            if (new_board.player == board.player) {
                choice = action;
                best_rank = kInfinity;
                sampler.count = 1;
                return false;
            }
            return true;
        }

        double rank = ClimbRank(board.player, new_board, weights);
        if (rank == best_rank) {
            if (sampler(random)) choice = action;
        } else if (rank > best_rank) {
            best_rank = rank;
            choice = action;
            sampler.count = 1;
        }
        return true;
    });
    Check(sampler.count > 0);
    return choice;
}
