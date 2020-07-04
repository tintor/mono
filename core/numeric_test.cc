#include "core/numeric.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("FOR", "[FOR]") {
    int a = 0;
    FOR(x, 3) FOR(y, 2) a += 1;
    REQUIRE(a == 6);
}
