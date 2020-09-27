#pragma once
#include "sokoban/level.h"

struct LevelEnv;
const Level* LoadLevel(const LevelEnv& level_env, bool extra = true);
const Level* LoadLevel(string_view filename);
void Destroy(const Level*);
