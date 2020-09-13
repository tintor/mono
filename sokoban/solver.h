#pragma once
#include "sokoban/level_env.h"
#include "sokoban/level.h"
#include "sokoban/state.h"
#include <vector>

using Solution = std::vector<DynamicState>;

struct SolverOptions {
    int verbosity = 2;
    bool single_thread = false;
    int dist_w = 1;
    int heur_w = 2;
    bool alt = false;
    bool monitor = true;
};

std::pair<std::vector<int2>, int> Solve(LevelEnv env, const SolverOptions& options);

void GenerateDeadlocks(const Level* level, const SolverOptions& options);
