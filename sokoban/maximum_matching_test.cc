#include "sokoban/maximum_matching.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("maximum_matching EASY", "") {
    BipGraph g(4, 4);
    g.addEdge(1, 4);
    g.addEdge(2, 3);
    g.addEdge(3, 2);
    g.addEdge(4, 1);
    REQUIRE(g.hopcroftKarp() == 4);
}

TEST_CASE("maximum_matching B", "") {
    BipGraph g(4, 4);
    g.addEdge(1, 1);
    g.addEdge(1, 2);
    g.addEdge(1, 4);
    g.addEdge(2, 2);
    g.addEdge(2, 3);
    g.addEdge(3, 3);
    g.addEdge(4, 3);
    REQUIRE(g.hopcroftKarp() == 3);
}

TEST_CASE("maximum_matching FULL", "") {
    BipGraph g(4, 4);
    for (int a = 1; a <= 4; a++) for (int b = 1; b <= 4; b++) g.addEdge(a, b);
    REQUIRE(g.hopcroftKarp() == 4);
}
