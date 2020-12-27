#include <random>

#include "fmt/core.h"
#include "core/random.h"

#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/reservoir_sampler.h"

using namespace std;

// Computer interface
// ==================

std::mt19937_64& Random() {
    static atomic<size_t> seed = 0;
    thread_local bool initialized = false;
    thread_local std::mt19937_64 random;
    if (!initialized) {
        random = std::mt19937_64(seed++);
        initialized = true;
    }
    return random;
}

int RandomInt(int count, std::mt19937_64& random) { return std::uniform_int_distribution<int>(0, count - 1)(random); }

double RandomDouble(std::mt19937_64& random) { return std::uniform_real_distribution<double>(0, 1)(random); }

int ChooseWeighted(double u, cspan<double> weights) {
    FOR(i, weights.size()) {
      if (u < weights[i]) return i;
      u -= weights[i];
    }
    return weights.size() - 1;
}

// Random is used in case of ties.
template<typename T>
int Argmax(std::mt19937_64& random, cspan<T> values) {
    int best_i = -1;
    T best_value;
    ReservoirSampler sampler;

    FOR(i, values.size()) {
        if (sampler.count == 0 || values[i] > best_value) {
            sampler.count = 1;
            best_i = i;
            best_value = values[i];
            continue;
        }
        if (values[i] == best_value && sampler(Random())) best_i = i;
    }
    return best_i;
}

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

Step AutoRandom(const Board& board) {
    while (true) {
        Step step = RandomStep(board);
        if (IsValid(board, step)) return step;
    }
}

Step AutoGreedy(const Board& board) {
    auto& random = Random();
    Action temp;
    Step choice;
    ReservoirSampler sampler;
    AllValidStepSequences(board, temp, [&](const Action& action, const Board& new_board) {
        // Print(action);
        if (new_board.phase == Phase::GameOver && new_board.player == board.player) {
            choice = action[0];
            sampler.count = 1;
            return false;
        }
        if (new_board.phase != Phase::GameOver && sampler(random)) choice = action[0];
        return true;
    });
    Check(sampler.count > 0);
    return choice;
}

const double Pow10[] = {1, 10, 100, 1000};

double ClimbRank(Figure player, const Board& board) {
    double rank = 0;
    for (Coord e : kAll) {
        if (board(e).figure == player) rank += Pow10[int(board(e).level)];
        if (board(e).figure == Other(player)) rank -= Pow10[int(board(e).level)];
    }
    return rank;
}

// Heuristics to use:
// - points for every worker which is not blocked
// - number of free cells around every worker (if they are not too tall)
// - number of squares that my workers can reach in 1 step that opponent's workers can't reach in 2 action? (breaks for Artemis and Hermes)
// - number of non-domed and non-occupied levels within 1 step from my workers
// - number of non-domed and non-occupied levels within 2 action from my workers (that were not included in above)

Step AutoClimber(const Board& board) {
    auto& random = Random();
    Action temp;
    Step choice;
    ReservoirSampler sampler;
    double best_rank = -1e100;
    AllValidStepSequences(board, temp, [&](const Action& action, const Board& new_board) {
        // Print(action);
        if (new_board.phase == Phase::GameOver) {
            if (new_board.player == board.player) {
                choice = action[0];
                best_rank = 1e100;
                sampler.count = 1;
                return false;
            }
            return true;
        }

        double rank = ClimbRank(board.player, new_board);
        if (rank == best_rank && sampler(random)) choice = action[0];
        if (rank > best_rank) {
            best_rank = rank;
            choice = action[0];
            sampler.count = 1;
        }
        return true;
    });
    Check(sampler.count > 0);
    return choice;
}
