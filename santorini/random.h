#pragma once
#include "core/random.h"
#include "core/numeric.h"

std::mt19937_64& Random();

inline int RandomInt(int count, std::mt19937_64& random) { return std::uniform_int_distribution<int>(0, count - 1)(random); }

inline int ChooseWeighted(double u, cspan<double> weights) {
    FOR(i, weights.size()) {
      if (u < weights[i]) return i;
      u -= weights[i];
    }
    return weights.size() - 1;
}
