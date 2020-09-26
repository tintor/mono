#include "sokoban/state.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

template<typename Boxes>
void Test(int num_boxes, int num_cells) {
    Boxes boxes;
    REQUIRE(!boxes[1]);

    boxes.set(1);
    boxes.print();
    boxes.set(2);
    boxes.print();
    boxes.set(4);
    boxes.print();
    boxes.set(7);
    boxes.print();
    REQUIRE(boxes[4]);

    REQUIRE(boxes[1]);
    boxes.reset(1);

    //REQUIRE(!boxes[0]);
    REQUIRE(!boxes[1]);
    REQUIRE(boxes[2]);
    REQUIRE(!boxes[3]);
    REQUIRE(boxes[4]);
    REQUIRE(!boxes[5]);
    REQUIRE(!boxes[6]);
    REQUIRE(boxes[7]);
}

TEST_CASE("DynamicBoxes", "") {
    Test<DynamicBoxes>(4, 8);
}

TEST_CASE("DenseBoxes<32>", "") {
    Test<DenseBoxes<32>>(4, 8);
}
