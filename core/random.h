#pragma once
#include <random>

#include "core/span.h"

using RandomEngine = std::mt19937_64;

template<typename Real>
typename std::enable_if_t<(std::is_floating_point_v<Real>), size_t>
NormalizedWeightedSample(cspan<Real> weights, RandomEngine& re) {
    Real p = std::uniform_real_distribution<Real>(0, 1)(re);
    for (size_t i = 0; i < weights.size(); i++) {
        if (p < weights[i]) return i;
        p -= weights[i];
    }
    return weights.size() - 1;
}
