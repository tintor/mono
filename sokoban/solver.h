#pragma once
#include "sokoban/level_env.h"
#include "sokoban/level.h"
#include "sokoban/state.h"
#include <vector>

using Solution = std::vector<DynamicState>;

std::pair<std::vector<int2>, int> Solve(LevelEnv env, int verbosity, bool single_thread);

void GenerateDeadlocks(const Level* level, bool single_thread);
