#include "sokoban/solver.h"
#include "sokoban/level_env.h"

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "core/vector.h"
#include "core/auto.h"
#include "core/bits_util.h"
#include "core/range.h"
#include "core/string.h"
#include "core/thread.h"
#include "core/timestamp.h"
using namespace std;

const std::vector<string_view> Blacklist = {
    "original:24",    // syntax?
    "microban2:131",  // takes 15+ minutes
    "microban3:47",   // takes 2+ hours
    "microban3:58",   // takes 10+ minutes
    "microban4:75",   // takes 1+ hours
    "microban4:85",   // takes 1.5+ hours
    "microban4:96",   // takes .5+ hours
    "microban5:26",
};

bool Blacklisted(string_view name) {
    return contains(Blacklist, name);
}

constexpr string_view prefix = "sokoban/levels/";

constexpr bool kAnimateSolution = false;

string Solve(string_view file, int verbosity) {
    Timestamp start_ts;
    atomic<int> total = 0;
    atomic<int> completed = 0;
    mutex levels_lock;

    vector<string> skipped;
    vector<string> unsolved;

    vector<string> levels;
    if (file.find(":"sv) != string_view::npos) {
        levels.push_back(cat(prefix, file));
    } else {
        auto num = NumberOfLevels(cat(prefix, file));
        for (int i = 1; i <= num; i++) {
            string name = fmt::format("{}:{}", file, i);
            if (Blacklisted(name)) {
                skipped.emplace_back(split(name, {':', '/'}).back());
                continue;
            }
            levels.push_back(fmt::format("{}{}", prefix, name));
        }
    }

    parallel_for(levels.size(), 1, [&](size_t task) {
        auto name = levels[task];
        total += 1;

        fmt::print("Level {}\n", name);
        LevelEnv env;
        env.Load(name);
        const auto solution = Solve(env, verbosity);
        if (!solution.first.empty()) {
            completed += 1;
            fmt::print("{}: solved in {} steps / {} pushes!\n", name, solution.first.size(), solution.second);
            if (kAnimateSolution) {
                env.Print();
                std::this_thread::sleep_for(100ms);
                env.Unprint();
                for (const int2 delta : solution.first) {
                    env.Action(delta);
                    env.Print();
                    std::this_thread::sleep_for(100ms);
                    env.Unprint();
                }
                if (!env.IsSolved()) THROW(runtime_error2, "not solved");
            }
        } else {
            fmt::print("{}: no solution!\n", name);
            unique_lock g(levels_lock);
            unsolved.emplace_back(split(name, {':', '/'}).back());
        }
        fmt::print("\n");
    });

    sort(unsolved, natural_less);
    sort(skipped, natural_less);

    string result;
    result += fmt::format("solved {}/{} in {:.3f}", completed, total, start_ts.elapsed_s());
    result += fmt::format(" unsolved {}", unsolved);
    result += fmt::format(" skipped {}", skipped);
    return result;
}

int Main(cspan<string_view> args) {
    if (args.size() == 2 && args[0] == "dbox2") {
        auto level = LoadLevel(cat(prefix, args[1]));
        PrintInfo(level);
        GenerateDeadlocks(level);
        return 0;
    }
    if (args.size() == 2 && args[0] == "scan") {
        auto num = NumberOfLevels(cat(prefix, args[1]));
        for (size_t i = 0; i < num; i++) {
            string name = fmt::format("{}:{}", args[1], i + 1);
            auto level = LoadLevel(cat(prefix, name));
            if (level) PrintInfo(level);
        }
        return 0;
    }
    if (args.size() == 0) {
        vector<string> results;
        for (auto file : {"microban1", "microban2", "microban3"}) //, "microban4", "microban5"})
            results.emplace_back(Solve(file, 2));
        for (auto result : results) fmt::print("{}\n", result);
        return 0;
    }
    if (args.size() == 1) {
        fmt::print("{}\n", Solve(args[0], 2));
        return 0;
    }
    return 0;
}

int main(int argc, char** argv) {
    InitSegvHandler();
    // Timestamp::init();

    vector<string_view> args;
    for (int i = 1; i < argc; i++) args.push_back(argv[i]);
    return Main(args);
}
