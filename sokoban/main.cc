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

const cspan<string_view> Blacklist = {
    "original:24",    // syntax?
    "microban2:131",  // takes 15+ minutes
    "microban2:132",  // large maze with one block
    "microban3:47",   // takes 2+ hours
    "microban3:58",   // takes 10+ minutes
    "microban4:75",   // takes 1+ hours
    "microban4:85",   // takes 1.5+ hours
    "microban4:92",   // deadlocks easily
    "microban4:96",   // takes .5+ hours
    "microban5:26",
};

constexpr string_view prefix = "sokoban/levels/";

string Solve(string_view file) {
    Timestamp start_ts;
    atomic<int> total = 0;
    atomic<int> completed = 0;
    vector<const Level*> levels;
    mutex levels_lock;

    vector<string> skipped;
    vector<string> unsolved;

    if (file.find(":"sv) != string_view::npos) {
        auto level = LoadLevel(cat(prefix, file));
        levels.push_back(level);
    } else {
        auto num = NumberOfLevels(cat(prefix, file));
        parallel_for(num, 1, [&](size_t task) {
            string name = fmt::format("{}:{}", file, task + 1);
            if (contains(Blacklist, string_view(name))) {
                unique_lock g(levels_lock);
                skipped.emplace_back(split(name, {':', '/'}).back());
                return;
            }

            auto level = LoadLevel(cat(prefix, name));
            unique_lock g(levels_lock);
            levels.push_back(level);
        });
        sort(levels, [](const Level* a, const Level* b) { return natural_less(a->name, b->name); });
    }

    parallel_for(levels.size(), 1, [&](size_t task) {
        auto level = levels[task];

        PrintInfo(level);
        total += 1;

        const auto solution = Solve(level);  // TODO pass option 2
        if (!solution.empty()) {
            completed += 1;
            fmt::print("{}: solved in {} pushes!\n", level->name, solution.size());
            if (false) {
                for (const auto& s : solution) Print(level, s);
            }
        } else {
            fmt::print("{}: no solution!\n", level->name);
            unique_lock g(levels_lock);
            unsolved.emplace_back(split(level->name, {':', '/'}).back());
        }
        fmt::print("\n");
    });

    sort(unsolved, natural_less);
    sort(skipped, natural_less);

    string result;
    // TODO .elapsed() uses %T custom format!
    result += fmt::format("solved {}/{} in {}", completed, total, start_ts.elapsed());
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
        for (auto file : {"microban1", "microban2", "microban3", "microban4", "microban5"})
            results.emplace_back(Solve(file));
        for (auto result : results) fmt::print("{}\n", result);
        return 0;
    }
    if (args.size() == 1) {
        fmt::print("{}\n", Solve(args[0]));
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
