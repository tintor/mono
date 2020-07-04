#include "core/string_util.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
using namespace std;

TEST_CASE("split", "[util]") {
    using namespace std::literals;
    REQUIRE(split(""sv) == vector<string_view>{});
    REQUIRE(split("x"sv) == vector<string_view>{"x"sv});
    REQUIRE(split(" a ana b[anana  "sv) == vector<string_view>{"a"sv, "ana"sv, "b[anana"sv});

    REQUIRE(split(" an|a ba|na"sv) == vector<string_view>{"an|a"sv, "ba|na"sv});
    REQUIRE(split(" an|a ba|na"sv, '|') == vector<string_view>{" an"sv, "a ba"sv, "na"sv});
}

TEST_CASE("natural_less", "[string_util]") {
    using namespace std::literals;
    REQUIRE(natural_less("ma2", "ma10"));
    REQUIRE(!natural_less("ma10", "ma2"));
    REQUIRE(!natural_less("ma2", "ma2"));
    REQUIRE(natural_less("a", "b"));
}

TEST_CASE("cat", "[string_util]") {
    using namespace std::literals;
    REQUIRE("abcde" == cat("abc"sv, "de"s));
    REQUIRE("abcde" == cat("abc"sv, "de"sv));
}
