#pragma once
#include "core/matrix.h"
#include <string_view>

struct LevelEnv {
    std::string name;

    // All matrices are the same size
    matrix<bool> wall;
    matrix<bool> box;
    matrix<bool> goal;
    matrix<bool> sink;
    int2 agent;

    void Reset(int rows, int cols);
    void Load(std::string_view filename);
    bool IsValid() const; // Valid doesn't imply solvable!
    void Print(bool edge = true) const;
    void Unprint() const;

    bool Action(int2 delta, bool allow_move = true, bool allow_push = true);
    bool Push(int2 delta) { return Action(delta, false, true); }
    bool Move(int2 delta) { return Action(delta, true, false); }

    bool IsSolved() const;
    bool ContainsSink() const;
};

int NumberOfLevels(std::string_view filename);
