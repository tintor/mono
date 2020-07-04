#include "core/diff.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("diff: ComputePolynomial", "[diff]") {
    std::vector<float> poly = {0, 1, 0, -1.f/(2*3), 0, 1.f/(2*3*4*5)};
    for (float x = 0; x <= 1; x += 0.01) {
        const float e = std::abs(sin(x) - ComputePolynomial(x, poly));
        REQUIRE(e <= 0.000196f);
    }
}

TEST_CASE("diff: ComputePolynomialDeriv", "[diff]") {
    std::vector<float> poly = {0, 1, 0, -1.f/(2*3), 0, 1.f/(2*3*4*5)};
    for (float x = 0; x <= 1; x += 0.01) {
        const float e = std::abs(cos(x) - ComputePolynomialDeriv(x, poly));
        REQUIRE(e <= 0.00137f);
    }
}
