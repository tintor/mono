#include "core/zip.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
using namespace std;

TEST_CASE("", "[zip]") {
    vector<int> a = {1, 2, 3, 4};
    vector<int> b = {-2, -4, -8};

    vector<pair<int, int>> actual;
    for (auto p : zip(span<int>(a), span<int>(b))) actual.push_back(p);

    vector<pair<int, int>> expected = {{1, -2}, {2, -4}, {3, -8}};
    REQUIRE(expected == actual);
}
