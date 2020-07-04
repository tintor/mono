#include "core/numeric.h"
#include "core/array_deque.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("array_deque basic", "[array_deque]") {
    array_deque<int> deque;
    REQUIRE(deque.capacity() == 0);

    FOR(i, 100) {
        REQUIRE(deque.size() == i);
        deque.push_back(i);
    }
    REQUIRE(deque.capacity() == 128);

    FOR(i, 40) REQUIRE(i == deque.pop_front());

    REQUIRE(deque.size() == 60);
    REQUIRE(deque.capacity() == 128);

    FOR(i, 70) deque.push_back(100 + i);
    REQUIRE(deque.size() == 130);

    FOR(i, 130) REQUIRE(deque[i] == 40 + i);
}
