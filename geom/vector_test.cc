#include "geom/vector.h"
#include "fmt/core.h"
#include "fmt/ostream.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <strstream>
using namespace std::literals;

TEST_CASE("vector format", "[vector]") {
    std::ostrstream q;
    q << i3(3, 2, 1);
    REQUIRE(q.str() == "3 2 1"s);
    REQUIRE(fmt::format("{}", i3(3, 2, 1)) == "3 2 1");
    REQUIRE(fmt::format("{}", d3(3, -2.1, 0)) == "3 -2.1 0");
}

TEST_CASE("vector cast4", "[vector]") {
    double4 a = cast4(double3{1, 2, 3});
    REQUIRE(a.x == 1);
    REQUIRE(a.y == 2);
    REQUIRE(a.z == 3);

    float4 b = cast4(float3{2, 0, -8});
    REQUIRE(b.x == 2);
    REQUIRE(b.y == 0);
    REQUIRE(b.z == -8);
}

TEST_CASE("vector equal", "[vector]") {
    REQUIRE(equal(d3(1, 0, 2), d3(1, 0, 2)));
    REQUIRE(!equal(double3{1, 0, -2}, double3{1, 0, 2}));
    REQUIRE(!equal(double3{1, -3, 2}, double3{1, 3, 2}));
    REQUIRE(!equal(double3{-1, 0, 2}, double3{1, 0, 2}));
}
