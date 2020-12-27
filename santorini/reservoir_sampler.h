#pragma once
#include <random>

struct ReservoirSampler {
    std::uniform_real_distribution<double> dis;
    size_t count = 0;

    ReservoirSampler() : dis(0.0, 1.0) {}

    template <typename Random>
    bool operator()(Random& random) {
        return dis(random) * ++count <= 1.0;
    }
};
