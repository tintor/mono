#include "core/auto.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("auto basic", "[auto]") {
    int a = 0;
    {
        ON_SCOPE_EXIT(a += 1);
        REQUIRE(a == 0);
    }
    REQUIRE(a == 1);
}
