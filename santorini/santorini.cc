#include <random>
#include <variant>
#include <iostream>

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "core/random.h"
#include "core/callstack.h"
#include "core/model.h"
#include "core/thread.h"
#include "core/string.h"
#include "core/timestamp.h"
#include "core/vector.h"
#include "core/array_deque.h"

#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/minimax.h"
#include "santorini/mcts.h"
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

// Benchmarks and training
// -----------------------

using Policy = std::function<Step(const Board&)>;

Figure Battle(const Policy& policy_a, const Policy& policy_b) {
    Board board;
    while (true) {
        auto w = Winner(board);
        if (w != Figure::None) return w;
        const Policy& policy = (board.player == Figure::Player1) ? policy_a : policy_b;
        auto s = Execute(board, policy(board));
        if (s != nullopt) {
            print("faul\n");
            return Other(board.player);
        }
    }
}

const std::unordered_map<string_view, Policy> g_policies = {
    {"random", AutoRandom},
    {"greedy", AutoGreedy},
    {"climber", AutoClimber},
    {"mcts100", [](const auto& e) { return AutoMCTS(e, 100); }},
    {"mcts100t", [](const auto& e) { return AutoMCTS(e, 100, true); }},
    {"mcts200", [](const auto& e) { return AutoMCTS(e, 200); }},
    {"mcts400", [](const auto& e) { return AutoMCTS(e, 400); }},
    {"minimax6", [](const auto& e) { return AutoMiniMax(e, 6); }},
    {"minimax9", [](const auto& e) { return AutoMiniMax(e, 9); }},
    {"minimax12", [](const auto& e) { return AutoMiniMax(e, 12); }}};

void AutoBattle(int count, string_view name_a, string_view name_b) {
    const Policy& policy_a = g_policies.at(name_a);
    const Policy& policy_b = g_policies.at(name_b);
    atomic<int> wins_a = 0, wins_b = 0;

    atomic<bool> stop = false;
    thread monitor([&]() {
        string message;
        while (!stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            FOR(j, message.size()) cout << '\r';
            message = format("{} {} : {} {}", name_a, wins_a, wins_b, name_b);
            cout << message;
            cout.flush();
        }
        cout << '\n';
    });

    parallel_for(count, [&](size_t i) {
        if (Battle(policy_a, policy_b) == Figure::Player1)
            wins_a += 1;
        else
            wins_b += 1;
        if (Battle(policy_b, policy_a) == Figure::Player1)
            wins_b += 1;
        else
            wins_a += 1;
    });

    stop = true;
    monitor.join();
}

struct Agent {
    virtual Action Play(const Board& board) = 0;
};

struct SARS {
    Board state;
    Board step; // board state after agent's turn, but before opponent's turn (step + state)
    float reward;
    optional<Board> next_state; // board state after opponent's turn (or nullopt if game ends after agent's or opponent's turns)
};

class ReplayBuffer {
public:
    ReplayBuffer(int capacity) { buffer.reserve(capacity); }

    vector<SARS> Sample(int count) const {
        std::mt19937_64& random = Random();
        if (buffer.size() < count) count = buffer.size();

        vector<int> selected(count);
        for (int i = 0; i < count; i++) {
            while (true) {
                int e = RandomInt(buffer.size(), random);
                if (contains(cspan<int>(selected.data(), i), e)) continue;
                selected[i] = e;
                break;
            }
        }

        vector<SARS> sample(count);
        for (int i = 0; i < count; i++) sample[i] = buffer[selected[i]];
        return sample;
    }

    void Add(const SARS& e) {
        if (buffer.size() == buffer.capacity()) buffer.pop_front();
        buffer.push_back(e);
    }

private:
    mt::array_deque<SARS> buffer;
};

// network which learns Board -> float mapping
class BoardNet {
public:
    BoardNet() {
        in = Data({5, 5, 7}) << "input";
        ref = Data({1}) << "reference";
        auto w_init = make_shared<NormalInit>(0.01, Random());

        auto x = in;
        x = Conv2D(x, ConvType::Same, dim4(16, 1, 1, 7, 'i', 'w', 'h', 'c'), w_init);
        //b = Conv2D(x, ConvType::Same, dim4(16, 3, 3, 7, 'i', 'w', 'h', 'c'), w_init);
        //x = Concat(a, b);
        x = BatchNorm(x, 1e-8);
        x = Relu(x);
        x = Conv2D(x, ConvType::Valid, dim4(1, 5, 5, 32, 'i', 'w', 'h', 'c'), w_init);
        x = BatchNorm(x, 1e-8);
        out = Logistic(x, 15);

        loss = MeanSquareError(ref, out);
        model = Model({loss});
        model.Print();
        model.optimizer = std::make_shared<Optimizer>();
        model.optimizer->alpha = env("alpha", 0.01);
    }

    const vector<float>& Eval(const vector<MiniBoard>& batch) {
        model.SetBatchSize(batch.size()); // TODO this might be slow if different
        FOR(i, batch.size()) ToTensor(batch[i], in->v.slice(i));
        model.Forward(/*training*/false);
        return out->v.vdata();
    }

    void Train(const vector<pair<MiniBoard, float>>& batch) {
        model.SetBatchSize(batch.size());
        FOR(i, batch.size()) {
            ToTensor(batch[i].first, in->v.slice(i));
            ref->v[i] = batch[i].second;
        }
        model.Forward(/*training*/true);
        model.Backward(loss);
    }

private:
    Diff in, ref, out, loss;
    Model model;
};

// TODO Use Prioritized Replay Buffer (select SARS transitions with the worst fit).
// TODO Use Double DQN: q1 and q2 (instead of q_policy and q_train), and flip them randomly.
// TODO Can we use Dueling DQN here (with variable number of action)?
class DQNAgent : public Agent {
public:
    const double boltzmann_tau = 1;
    const float gamma = 0.999;

    bool _explore = true;

    // Inference only. No policy improvement or sample collection.
    Action Play(const Board& board) override {
        vector<MiniBoard> all_boards;
        vector<Action> all_action;

        AllValidStepSequences(board, _temp, [&](Action& action, const Board& new_board) {
            all_boards << static_cast<MiniBoard>(new_board);
            all_action << action;
            return true;
        });

        if (all_boards.empty()) return {};
        return all_action[ChooseStep(_q_policy.Eval(all_boards))];
    }

    // TODO How are transitions collected?
    // TODO How is training invoked?
    // TODO This training can happen in PyTorch python script on carbide, while network could be loaded into C++ for inference.
    void ImprovePolicy(const Figure ego) {
        const Figure opponent = Other(ego);
        const int sample_size = 16;
        const int K = 1000;

        FOR(i, K) {
            _xy.clear();
            // TODO Collect all new boards from double nested loop, and do all evals at once.
            // TODO Parallelize this loop.
            for (const auto& [state, step, reward, next_state] : _replay_buffer.Sample(sample_size)) {
                float max_q = 0;
                if (next_state.has_value()) {
                    max_q = -1;
                    AllValidStepSequences(next_state.value(), _temp, [&](Action& action, const Board& new_board) {
                        auto winner = Winner(new_board);
                        if (winner == ego) {
                          max_q = 1;
                        } else if (winner != opponent) {
                          max_q = std::max(max_q, _q_policy.Eval({new_board})[0]);
                        }
                        return true;
                    });
                    // If no possible move then game is lost and max_q == -1.
                }
                _xy.emplace_back(static_cast<MiniBoard>(step), reward + gamma * max_q);
            }
            _q_train.Train(_xy); // with Huber loss (quadratic for small errors, linear for large errors)
        }
        // TODO evaluate loss against the entire replay_buffer

        // TODO Copy weights from q_train to q_policy.
    }

private:
    int ChooseStep(const vector<float>& q_values) {
        if (!_explore) return Argmax(Random(), cspan<float>(q_values));

        // Subtract average to reduce chance of overflow.
        // TODO Subtracting max value might be better.
        double avg = Sum(q_values, 0.0) / q_values.size();
        vector<double> exp_q_values;
        for (float v : q_values) exp_q_values << exp((v - avg) * boltzmann_tau);
        const double u = RandomInt(q_values.size(), Random()) * Sum(exp_q_values, 0.0);
        return ChooseWeighted(u, exp_q_values);
    }

    ReplayBuffer _replay_buffer;
    BoardNet _q_train, _q_policy;

    // Cached vectors to avoid reallocation.
    vector<pair<MiniBoard, float>> _xy;
    Action _temp;
};

struct RandomAgent : public Agent {
    Action Play(const Board& board) override {
        Action action;
        Board my_board = board;
        while (true) {
            Step step = AutoRandom(my_board);
            action << step;
            Execute(my_board, step);
            if (std::holds_alternative<NextStep>(step) || Winner(my_board) != Figure::None) break;
        }
        return action;
    }
};

struct GreedyAgent : public Agent {
    Action Play(const Board& board) override {
        Action action;
        Board my_board = board;
        while (true) {
            Step step = AutoGreedy(my_board);
            action << step;
            Execute(my_board, step);
            if (std::holds_alternative<NextStep>(step) || Winner(my_board) != Figure::None) break;
        }
        return action;
    }
};

// Plan:
// - value function is based on the entire turn (1 or more action of same player)
// - play both as player1 and player2
// - keep accumulating experience (and save it to file)
// - play games in parallel
// - after every 1000 games train new network (from scratch?) (using all previous experience)
// - if new network plays worse after 1000 games -> discard it
// - port to carbide!

// Network architecture:
// - board is 5x5x7 bits without any extra bits
// : layer0
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer1
// - Concat(in)
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer2
// - Concat(in, layer1)
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer3
// - Concat(in, layer1, layer2)
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer4
// - Concat(in, layer1, layer2, layer3)
// - Valid Conv2D 3x3 with 10 channels -> 3x3xM
// - BatchNorm
// - Relu
// : layer5
// - Flatten
// - Dense
// - BatchNorm
// - Relu
// - Dense
// - Sigmoid

Figure PlayOneGame(Board board, Agent& agent_a, Agent& agent_b, Values& values, vector<Board>& history) {
    history.clear();

    Figure w = Winner(board);
    while (w == Figure::None) {
        Agent& agent = (board.player == Figure::Player1) ? static_cast<Agent&>(agent_a) : static_cast<Agent&>(agent_b);

        bool eot = false;
        for (Step step : agent.Play(board)) {
            if (eot) Fail("invalid step after end of turn");
            auto s = Execute(board, step);
            if (s != nullopt) Fail(format("invalid step {}", s));
            eot = std::holds_alternative<NextStep>(step) || Winner(board) != Figure::None;
        }
        if (!eot) Fail("player didn't end turn");

        history << board;
        w = Winner(board);
    }

    Figure p = Figure::Player1;
    for (const Board& board : history) {
        values.Add(board, Score(w));
        p = board.player;
    }
    return w;
}

Score PlayManyGames(Agent& agent_a, Agent& agent_b, const size_t tasks, const size_t task_size, Values& values) {
    atomic<size_t> next = 0, completed = 0;
    Score score;
    mutex score_mutex;

    parallel([&]() {
        Values local_values;
        Score local_score;
        vector<Board> history;
        for (size_t task = next++; task < tasks; task = next++) {
            local_values.Clear();
            FOR(i, task_size) {
                auto w = PlayOneGame(Board(), agent_a, agent_b, local_values, history);
                if (w == Figure::Player1) local_score.p1 += 1;
                if (w == Figure::Player2) local_score.p2 += 1;
            }
            values.Merge(local_values);
            auto c = ++completed;
            if (c % 50 == 0) {
                std::unique_lock lock(g_cout_mutex);
                print("started {}, finished {}\n", next.load(), c);
            }
        }
        std::unique_lock lock(score_mutex);
        score += local_score;
    });
    return score;
}

void Learn(Values& values) {
    RandomAgent agent_a;
    //MonteCarloAgent agent_a(0.01, true, 100);

    //MonteCarloAgent agent_b(0.01, false, 1000);
    RandomAgent agent_b;
    //GreedyAgent agent_b;

    std::ofstream stats("stats.txt");

    Timestamp begin;
    PlayManyGames(agent_a, agent_b, 100, 2000, values);
    Timestamp end;
    print("elapsed {}, states {}\n", begin.elapsed_s(end), values.Size());

    // self-play
/*    double ratio = 0.5;
    Values values;
    vector<Board> history;
    size_t db = 0;
    while (true) {
        auto winner = PlayOneGame(agent_a, agent_b, history);

        Figure p = Figure::Player1;
        for (const Board& board : history) {
            values.Add(board, (winner == p) ? 1 : 0, 1);
            p = board.player;
        }

        constexpr double q = 0.0025;
        ratio = ratio * (1 - q) + (winner == Figure::Player1 ? 1 : 0) * q;
        if ((game + 1) % 10000 == 0) println("Game %s (wins %.4f, values %s)", game + 1, ratio, values.Size());

        if ((game + 1) % 100000 == 0) {
            println("Saving after %s games", game + 1);
            values.Export(format("db_%s.values", ++db));
        }
    }*/
}

void Browse(const Values& values) {
    vector<Board> stack = { Board() };
    while (true) {
        print("\n");
        Render(stack.back());
        Action temp;
        vector<Board> options;
        AllValidStepSequences(stack.back(), temp, [&](Action& action, const Board& new_board) {
            if (values.Lookup(new_board).has_value()) options.push_back(new_board);
            return true;
        });

        int id = 0;
        for (const Board& b : options) {
            auto score = values.Lookup(b);
            column_section(10, 15);
            print("[{}]", id);
            if (score) print("{} {}", score->p1, score->p2); else print("\n");
            id += 1;
            Render(b);
        }
        end_column_section();

        while (true) {
            print("> ");
            std::cin >> id;
            if (-1 <= id && id < int(options.size())) break;
        }

        if (id == -1 && stack.size() > 1) stack.pop_back();
        if (id >= 0) stack.push_back(options[id]);
    }
}

void RunUI();

int main(int argc, char** argv) {
    InitSegvHandler();

    if (argc > 1 && argv[1] == "learn"s) {
        Values values;
        Learn(values);
        values.Export("random_vs_random.values");
        return 0;
    }

    if (argc > 1 && argv[1] == "browse"s) {
        Values values("random_vs_random.values");
        Browse(values);
        return 0;
    }

    if (argc > 1 && argv[1] == "combo"s) {
        Values values;
        Learn(values);
        Browse(values);
        return 0;
    }

    if (argc > 1) {
        AutoBattle(100, "climber", "minimax12");
        AutoBattle(1000, "mcts100", "mcts100t");
        AutoBattle(100, "minimax6", "minimax9");
        AutoBattle(100, "climber", "minimax9");
        AutoBattle(100, "climber", "minimax6");
        AutoBattle(100, "climber", "mcts100");
        AutoBattle(100, "climber", "mcts200");
        AutoBattle(100, "climber", "mcts400");
        return 0;
    }

    RunUI();
    return 0;
}
