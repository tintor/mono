#include "core/tensor.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("dim4 ndims", "[tensor]") {
    REQUIRE(dim4{}.ndims() == 0);
    REQUIRE(dim4{1}.ndims() == 1);
    REQUIRE(dim4{3}.ndims() == 1);
    REQUIRE(dim4{1, 3, 1}.ndims() == 3);
}

TEST_CASE("dim4 elements", "[tensor]") {
    REQUIRE(dim4{1}.elements() == 1);
    REQUIRE(dim4{5, 3}.elements() == 15);
    REQUIRE(dim4{2, 2, 2}.elements() == 8);
    REQUIRE(dim4{2, 3, 4, 5}.elements() == 2 * 3 * 4 * 5);
}

TEST_CASE("dim4 ==", "[tensor]") {
    REQUIRE(dim4{2} == dim4{2});
    REQUIRE(dim4{2, 2} == dim4{2, 2});
    REQUIRE(dim4{2, 1} != dim4{2});
    REQUIRE(dim4{2, 2, 0, 0, 'B'} != dim4{2, 2});
    REQUIRE(dim4{2, 2, 0, 0, 'B'} == dim4{2, 2, 0, 0, 'B'});
}

TEST_CASE("dim4 pop_front", "[tensor]") {
    REQUIRE(dim4{2, 1}.pop_front() == dim4{1});
    REQUIRE(dim4{2}.pop_front() == dim4{});
    REQUIRE(dim4{}.pop_front() == dim4{});

    REQUIRE(dim4{2, 5, 3}.pop_front().ndims() == 2);
    REQUIRE(dim4{2, 5, 3}.pop_front().elements() == 15);
}

TEST_CASE("dim4 pop_back", "[tensor]") {
    REQUIRE(dim4{2, 1}.pop_back() == dim4{2});
    REQUIRE(dim4{2}.pop_back() == dim4{});
    REQUIRE(dim4{}.pop_back() == dim4{});

    REQUIRE(dim4{2, 5, 3}.pop_back().ndims() == 2);
    REQUIRE(dim4{2, 5, 3}.pop_back().elements() == 10);
}


TEST_CASE("dim4 str", "[tensor]") {
    REQUIRE(dim4{2, 4, 5}.str() == "[2 4 5]");
    REQUIRE(dim4{2}.str() == "[2]");
    REQUIRE(dim4{1}.str() == "[1]");
    REQUIRE(dim4{}.str() == "[]");
    REQUIRE(dim4{2, 4, 5, 0, 'b', ' ', 'h'}.str() == "[2b 4 5h]");
}

TEST_CASE("dim4 push_front", "[tensor]") {
    REQUIRE(dim4{}.push_front(3) == dim4{3});
    REQUIRE(dim4{2, 1}.push_front(3) == dim4{3, 2, 1});
    REQUIRE(dim4{2, 1}.push_front(3).ndims() == 3);
    REQUIRE(dim4{2, 1}.push_front(3).elements() == 6);
}

TEST_CASE("dim4 push_back", "[tensor]") {
    REQUIRE(dim4{}.push_back(3) == dim4{3});
    REQUIRE(dim4{2, 4}.push_back(3) == dim4{2, 4, 3});
    REQUIRE(dim4{2, 4}.push_back(3).ndims() == 3);
    REQUIRE(dim4{2, 4}.push_back(3).elements() == 24);
}

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
