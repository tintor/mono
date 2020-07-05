#pragma once
#include "sokoban/level.h"
#include "sokoban/state.h"

using Solution = std::vector<DynamicState>;

Solution Solve(const Level* level);

void GenerateDeadlocks(const Level* level);
