#include "sokoban/solver.h"
#include "sokoban/level_env.h"

#include "core/fmt.h"
#include "core/vector.h"
#include "core/string.h"
#include "core/thread.h"
#include "core/timestamp.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <filesystem>
#include <iostream>
#include <fstream>
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

constexpr string_view kPrefix = "sokoban/levels/";

constexpr string_view kSolvedPath = "/tmp/sokoban/solved";

bool is_level_solved(string_view name) {
    return std::filesystem::exists(format("{}/{}", kSolvedPath, name));
}

void mark_level_solved(string_view name) {
    std::filesystem::create_directories(kSolvedPath);
    std::ofstream of(format("{}/{}", kSolvedPath, name), ios_base::app);
}

struct Options : public SolverOptions {
    bool unsolved = false;
    bool animate = false;
    bool must_solve = true;
};

string Solve(string_view file, const Options& options) {
    Timestamp start_ts;
    atomic<int> total = 0;
    atomic<int> completed = 0;
    mutex levels_lock;

    vector<string> skipped;
    vector<string> unsolved;

    vector<string> levels;
    if (file.find(":"sv) != string_view::npos) {
        levels.push_back(string(file));
    } else {
        auto num = NumberOfLevels(cat(kPrefix, file));
        for (int i = 1; i <= num; i++) {
            string name = format("{}:{}", file, i);
            if (!options.unsolved && contains(Blacklist, string_view(name))) {
                skipped.emplace_back(split(name, {':', '/'}).back());
                continue;
            }
            if (options.unsolved && is_level_solved(name)) {
                skipped.emplace_back(split(name, {':', '/'}).back());
                continue;
            }
            levels.push_back(name);
        }
    }

    parallel_for(levels.size(), 1, [&](size_t task) {
        auto name = levels[task];
        total += 1;

        print("Level {}\n", name);
        LevelEnv env;
        env.Load(format("{}{}", kPrefix, name));
        const auto solution = Solve(env, options);
        if (!solution.first.empty()) {
            completed += 1;
            print("{}: solved in {} steps / {} pushes!\n", name, solution.first.size(), solution.second);
            mark_level_solved(name);
            if (options.animate) {
                env.Print();
                std::this_thread::sleep_for(100ms);
                env.Unprint();
                for (const int2 delta : solution.first) {
                    env.Action(delta);
                    env.Print();
                    std::this_thread::sleep_for(60ms);
                    env.Unprint();
                }
                if (!env.IsSolved()) THROW(runtime_error2, "not solved");
            }
        } else {
            print("{}: no solution!\n", name);
            if (options.must_solve) THROW(runtime_error2, "no solution");
            unique_lock g(levels_lock);
            unsolved.emplace_back(split(name, {':', '/'}).back());
        }
        print("\n");
    });

    sort(unsolved, natural_less);
    sort(skipped, natural_less);

    string result;
    result += format("solved {}/{} in {:.3f}", completed, total, start_ts.elapsed_s());
    result += format(" unsolved {}", unsolved);
    result += format(" skipped {}", skipped);
    return result;
}

int main(int argc, char** argv) {
    InitSegvHandler();
    Timestamp::init();

    Options options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("max_time", po::value<int>(&options.max_time), "")
        ("debug", po::bool_switch(&options.debug), "")
        ("alt", po::bool_switch(&options.alt), "")
        ("animate", po::bool_switch(&options.animate), "")
        ("single-thread", po::bool_switch(&options.single_thread), "")
        ("unsolved", po::bool_switch(&options.unsolved), "")
        ("verbosity", po::value<int>(&options.verbosity), "")
        ("dist_w", po::value<int>(&options.dist_w), "")
        ("heur_w", po::value<int>(&options.heur_w), "")
        ("must_solve", po::value<bool>(&options.must_solve), "")
        ("monitor", po::value<bool>(&options.monitor), "")
        ("deadlocks", po::value<string>(), "")
        ("scan", po::value<string>(), "")
        ("open", po::value<string>(), "")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("deadlocks")) {
        auto level = LoadLevel(cat(kPrefix, vm["deadlocks"].as<string>()));
        PrintInfo(level);
        GenerateDeadlocks(level, {.single_thread = options.single_thread});
        return 0;
    }

    if (vm.count("scan")) {
        auto num = NumberOfLevels(cat(kPrefix, vm["scan"].as<string>()));
        for (size_t i = 0; i < num; i++) {
            string name = format("{}:{}", vm["scan"].as<string>(), i + 1);
            auto level = LoadLevel(cat(kPrefix, name));
            if (level) PrintInfo(level);
        }
        return 0;
    }

    if (vm.count("open")) {
        print("{}\n", Solve(vm["open"].as<string>(), options));
        return 0;
    }

    vector<string> results;
    for (auto file : {"microban1", "microban2", "microban3", "microban4", "microban5"})
        results.emplace_back(Solve(file, options));
    for (auto result : results) print("{}\n", result);
    return 0;
}
