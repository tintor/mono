#include "core/sparse_matrix.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace std;

// TODO make it an actual test
TEST_CASE("sparse_matrix simple", "[sparse_matrix]") {
    sparse_matrix<float, uint16_t> a = {{0, 0, 3}, {2, 3, 8}};
    sparse_matrix<float, uint16_t> b = {{1, 1, -1}};
    auto c = add(a, b);
    auto d = mul(a, b);
}
