#include "core/algorithm.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("min", "[util]") {
    REQUIRE(min(2, 1) == 1);
    REQUIRE(min(5, 7, 9) == 5);
    REQUIRE(min(9, 7, 5) == 5);
    REQUIRE(min(3, 0, -1, 6, 10, -4, 3) == -4);
}
