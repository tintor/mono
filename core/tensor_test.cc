#include "core/tensor.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("tensor", "[tensor]") {
    std::array<float, 6> m = {1, 2, 3, 4, 5, 6};
    std::array<float, 3> n = {1, 2, 3};
    REQUIRE(tensor(m.data(), {6})[2] == 3);

    REQUIRE(tensor(m.data(), {6})(3) == 4);
    REQUIRE(tensor(m.data(), {2, 3})(0, 0) == 1);
    REQUIRE(tensor(m.data(), {2, 3})(0, 1) == 2);
    REQUIRE(tensor(m.data(), {2, 3})(0, 2) == 3);
    REQUIRE(tensor(m.data(), {2, 3})(1, 0) == 4);
    REQUIRE(tensor(m.data(), {2, 3})(1, 1) == 5);
    REQUIRE(tensor(m.data(), {2, 3})(1, 2) == 6);

    REQUIRE(tensor(m.data(), {3}) == tensor(n.data(), {3}));
    REQUIRE(tensor(m.data() + 1, {3}) != tensor(n.data(), {3}));
    REQUIRE(tensor(m.data(), {3}) != tensor(n.data(), {2}));
    REQUIRE(tensor(m.data(), {3}) != tensor(n.data(), {3, 1}));

    REQUIRE(tensor(m.data(), {2, 3}).slice(0) == tensor(m.data(), {3}));
    REQUIRE(tensor(m.data(), {2, 3}).slice(1) == tensor(m.data() + 3, {3}));

    REQUIRE(tensor(m.data(), {2}));
    REQUIRE(!tensor());
    REQUIRE(!tensor(nullptr, dim4()));

    REQUIRE(tensor().elements() == 0);
    REQUIRE(tensor() == tensor());
    REQUIRE(tensor(m.data(), {1}) != tensor());
}

TEST_CASE("vtensor", "[tensor]") {
    std::vector<int> a;
    REQUIRE(a.data() == nullptr);
}
