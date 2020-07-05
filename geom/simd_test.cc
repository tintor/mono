#include "geom/simd.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("simd cast4", "[simd]") {
    double4 a = cast4(double3{1, 2, 3});
    REQUIRE(a.x == 1);
    REQUIRE(a.y == 2);
    REQUIRE(a.z == 3);

    float4 b = cast4(float3{2, 0, -8});
    REQUIRE(b.x == 2);
    REQUIRE(b.y == 0);
    REQUIRE(b.z == -8);
}

TEST_CASE("simd equal", "[simd]") {
    REQUIRE(equal(double3{1, 0, 2}, double3{1, 0, 2}));
    REQUIRE(!equal(double3{1, 0, -2}, double3{1, 0, 2}));
    REQUIRE(!equal(double3{1, -3, 2}, double3{1, 3, 2}));
    REQUIRE(!equal(double3{-1, 0, 2}, double3{1, 0, 2}));
}
