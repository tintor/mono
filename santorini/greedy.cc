#include <random>

#include "fmt/core.h"
#include "core/random.h"

#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/reservoir_sampler.h"

using namespace std;

// Computer interface
// ==================

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
    Check(sampler.count > 0);
    return choice.empty() ? loose_choice : choice;
}

const double LevelWeight[] = {0, 1, 10, 10};
const double kInfinity = std::numeric_limits<double>::infinity();

// TODO
// - points for every worker which is not fully blocked
// - number of free cells around every worker (if they are not too tall)
// - number of squares that my workers can reach in 1 step that opponent's workers can't reach in 2 action? (breaks for Artemis and Hermes)
// - number of non-domed and non-occupied levels within 1 step from my workers
// - number of non-domed and non-occupied levels within 2 action from my workers (that were not included in above)
// - fine tune weights using self-play
double ClimbRank(Figure player, const Board& board) {
    if (board.phase == Phase::GameOver) return (player == board.player) ? kInfinity : -kInfinity;

    double rank = 0;
    const Figure other = Other(player);
    for (Coord e : kAll) if (board(e).level != 0) {
        if (board(e).figure == player) rank += LevelWeight[int(board(e).level)];
        if (board(e).figure == other) rank -= LevelWeight[int(board(e).level)];
    }
    return rank;
}

Action AutoClimber(const Board& board) {
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

        double rank = ClimbRank(board.player, new_board);
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
