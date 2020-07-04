#include "core/range.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("range format") {
    REQUIRE(fmt::format("{}", range(5)) == "range(5)");
    REQUIRE(fmt::format("{}", range(1, 5)) == "range(1, 5)");
    REQUIRE(fmt::format("{}", range(0, 5, 2)) == "range(0, 5, 2)");
    REQUIRE(fmt::format("{}", range(1, 5, 2)) == "range(1, 5, 2)");
}

template <typename T>
auto collect(const range<T>& r) {
    std::vector<T> a;
    for (auto x : r) a.push_back(x);
    return a;
}

TEST_CASE("range simple") {
    REQUIRE(collect(range(4)) == std::vector<int>{0, 1, 2, 3});
    REQUIRE(collect(range(1, 4)) == std::vector<int>{1, 2, 3});
    REQUIRE(collect(range(4, 0, -1)) == std::vector<int>{4, 3, 2, 1});
    REQUIRE(collect(range<double>(1, 4, 0.5)) == std::vector<double>{1.0, 1.5, 2.0, 2.5, 3.0, 3.5});
}

TEST_CASE("range by_reference") {
    int b = 1;
    std::vector<int> a;
    for (auto x : range(b)) {
        a.push_back(x);
        if (x == 0) b += 1;
    }
    REQUIRE(a == std::vector<int>{0, 1});
}

TEST_CASE("range by_reference2") {
    int b = 3;
    bool e = true;
    std::vector<int> a;
    for (auto& x : range(b)) {
        a.push_back(x);
        if (x == 1 && e) {
            x -= 1;
            e = false;
        }
    }
    REQUIRE(a == std::vector<int>{0, 1, 1, 2});
}
