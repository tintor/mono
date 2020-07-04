#include "core/thread.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("parallel_map_reduce", "[thread]") {
    std::atomic<int> next = 0;
    int result = parallel_map_reduce<int>([&next](){ int a = ++next; return a; }, [](int a, int b) { return a + b; }, false);
    int c = next.load();
    REQUIRE(c * (c + 1) / 2 == result);
}
