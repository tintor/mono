#include "sokoban/maximum_matching.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("maximum_matching EASY", "") {
    BipartiteGraph g(4, 4);
    g.add_edge(1, 4);
    g.add_edge(2, 3);
    g.add_edge(3, 2);
    g.add_edge(4, 1);
    REQUIRE(g.maximum_matching() == 4);
}

TEST_CASE("maximum_matching B", "") {
    BipartiteGraph g(4, 4);
    g.add_edge(1, 1);
    g.add_edge(1, 2);
    g.add_edge(1, 4);
    g.add_edge(2, 2);
    g.add_edge(2, 3);
    g.add_edge(3, 3);
    g.add_edge(4, 3);
    REQUIRE(g.maximum_matching() == 3);
}

TEST_CASE("maximum_matching FULL", "") {
    BipartiteGraph g(4, 4);
    for (int a = 1; a <= 4; a++) for (int b = 1; b <= 4; b++) g.add_edge(a, b);
    REQUIRE(g.maximum_matching() == 4);
}
