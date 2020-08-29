#pragma once
#include "core/matrix.h"
#include <string_view>

struct LevelEnv {
    // All matrices are the same size
    matrix<bool> wall;
    matrix<bool> box;
    matrix<bool> goal;
    int2 agent;

    void Load(std::string_view filename);
    bool IsValid() const; // Valid doesn't imply solvable!
    void Print() const;
    bool Move(int dir); // Dir (0 right, 1 up, 2 left, 3 down)
    bool IsSolved() const;
};

int NumberOfLevels(std::string_view filename);
