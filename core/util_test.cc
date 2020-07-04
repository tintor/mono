#include "core/util.h"
#include "core/numeric.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <unordered_set>
#include <unordered_map>
#include <list>

TEST_CASE("util typename", "[util]") {
    REQUIRE(TypeName(int(2)) == "int");
    REQUIRE(TypeName(uint(2)) == "uint");
    REQUIRE(TypeName(cent(2)) == "cent");

    REQUIRE(TypeName(std::chrono::nanoseconds()) == "std::chrono::nanoseconds");

    REQUIRE(TypeName(std::optional<int>(2)) == "std::optional<int>");
    REQUIRE(TypeName(std::string()) == "std::string");
    REQUIRE(TypeName(std::string_view()) == "std::string_view");

    REQUIRE(TypeName(std::set<int>()) == "std::set<int>");
    REQUIRE(TypeName(std::multiset<int>()) == "std::multiset<int>");
    REQUIRE(TypeName(std::unordered_multiset<int>()) == "std::unordered_multiset<int>");
    REQUIRE(TypeName(std::unordered_set<int>()) == "std::unordered_set<int>");

    REQUIRE(TypeName(std::map<int, bool>()) == "std::map<int, bool>");
    REQUIRE(TypeName(std::multimap<std::string, bool>()) == "std::multimap<std::string, bool>");
    REQUIRE(TypeName(std::unordered_multimap<int, bool>()) == "std::unordered_multimap<int, bool>");
    REQUIRE(TypeName(std::unordered_map<int, bool>()) == "std::unordered_map<int, bool>");

    REQUIRE(TypeName(std::vector<int>()) == "std::vector<int>");
    REQUIRE(TypeName(std::list<int>()) == "std::list<int>");
}
