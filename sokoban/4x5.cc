#include "sokoban/level_env.h"
#include "sokoban/level.h"
#include "core/fmt.h"

void FindAll(int rows, int cols) {
    LevelEnv env;
    env.Reset(rows + 4, cols + 4);
    for (int r = 0; r < rows + 4; r++) {
        for (int c = 0; c < cols + 4; c++) {
            if (r == 0 || r == rows + 3 || c == 0 || c == cols + 3) {
                env.wall(r, c) = true;
                continue;
            }
            if (r == 1 || r == rows + 2 || c == 1 || c == cols + 2) {
                env.sink(r, c) = true;
                continue;
            }
        }
    }
    env.agent = {1, 1};
    env.Print();
}

int main(int argc, char* argv[]) {
    FindAll(2, 2);
    FindAll(2, 3);
    FindAll(2, 4);
    FindAll(3, 3);
    FindAll(2, 5);
    FindAll(3, 4);
    FindAll(4, 4);
    FindAll(3, 5);
    FindAll(4, 5);
    return 0;
}
