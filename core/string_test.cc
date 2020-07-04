#include <unordered_set>
#include <unordered_map>
#include <list>

#include "core/string.h"
#include "core/numeric.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace std;
using namespace std::literals;

TEST_CASE("TypeName", "[string]") {
    REQUIRE(TypeName(int(2)) == "int");
    REQUIRE(TypeName(uint(2)) == "uint");
    REQUIRE(TypeName(cent(2)) == "cent");

    REQUIRE(TypeName(chrono::nanoseconds()) == "std::chrono::nanoseconds");

    REQUIRE(TypeName(optional<int>(2)) == "std::optional<int>");
    REQUIRE(TypeName(string()) == "std::string");
    REQUIRE(TypeName(string_view()) == "std::string_view");

    REQUIRE(TypeName(set<int>()) == "std::set<int>");
    REQUIRE(TypeName(multiset<int>()) == "std::multiset<int>");
    REQUIRE(TypeName(unordered_multiset<int>()) == "std::unordered_multiset<int>");
    REQUIRE(TypeName(unordered_set<int>()) == "std::unordered_set<int>");

    REQUIRE(TypeName(map<int, bool>()) == "std::map<int, bool>");
    REQUIRE(TypeName(multimap<std::string, bool>()) == "std::multimap<std::string, bool>");
    REQUIRE(TypeName(unordered_multimap<int, bool>()) == "std::unordered_multimap<int, bool>");
    REQUIRE(TypeName(unordered_map<int, bool>()) == "std::unordered_map<int, bool>");

    REQUIRE(TypeName(vector<int>()) == "std::vector<int>");
    REQUIRE(TypeName(list<int>()) == "std::list<int>");
}

TEST_CASE("split", "[string]") {
    REQUIRE(split(""sv) == vector<string_view>{});
    REQUIRE(split("x"sv) == vector<string_view>{"x"sv});
    REQUIRE(split(" a ana b[anana  "sv) == vector<string_view>{"a"sv, "ana"sv, "b[anana"sv});

    REQUIRE(split(" an|a ba|na"sv) == vector<string_view>{"an|a"sv, "ba|na"sv});
    REQUIRE(split(" an|a ba|na"sv, '|') == vector<string_view>{" an"sv, "a ba"sv, "na"sv});
}

TEST_CASE("natural_less", "[string]") {
    REQUIRE(natural_less("ma2", "ma10"));
    REQUIRE(!natural_less("ma10", "ma2"));
    REQUIRE(!natural_less("ma2", "ma2"));
    REQUIRE(natural_less("a", "b"));
}

TEST_CASE("cat", "[string]") {
    REQUIRE("abcde" == cat("abc"sv, "de"s));
    REQUIRE("abcde" == cat("abc"sv, "de"sv));
}
