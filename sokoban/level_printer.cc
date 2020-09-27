#include "core/fmt.h"
#include "sokoban/level_printer.h"

inline double Choose(uint a, uint b) {
    double s = 1;
    // TODO can be done in single loop to avoid double and overflow!
    for (uint i = a; i > a - b; i--) s *= i;
    for (uint i = 1; i <= b; i++) s /= i;
    return s;
}

inline double Complexity(const Level* level) {
    return log((level->num_alive - level->num_boxes) * Choose(level->num_alive, level->num_boxes));
}

void PrintInfo(const Level* level) {
    print("alive {}, boxes {}, complexity {}\n", level->num_alive, level->num_boxes, (int)round(Complexity(level)));
    Print(level, level->start);
}
