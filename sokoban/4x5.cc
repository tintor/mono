#include "sokoban/level_env.h"
#include "sokoban/level.h"
#include "core/fmt.h"

using std::vector;

LevelEnv MakeEnv(int rows, int cols) {
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
    return env;
}

bool Increment(vector<char>& code, int* num_boxes) {
    size_t i = 0;
    while (i < code.size()) {
        char& c = code[i];
        if (c == 1) *num_boxes -= 1;
        c += 1;
        if (c == 1) *num_boxes += 1;
        if (c < 3) return true;
        c = 0;
        i += 1;
    }
    return false;
}

void Apply(const vector<char>& code, LevelEnv* env) {
    const int cols = env->wall.cols() - 4;
    for (int i = 0; i < code.size(); i++) {
        int2 m = {i % cols + 2, i / cols + 2};
        env->box(m) = code[i] == 1;
        env->wall(m) = code[i] == 2;
    }
}

bool IsSolveable(const LevelEnv& env) {
    // TODO
    return true;
}

// empty or one box
bool HasEmptyRowX(const LevelEnv& env, int row) {
    const int ce = env.wall.cols() - 2;
    bool box = false;
    for (int c = 2; c < ce; c++) {
        if (env.wall(row, c)) return false;
        if (env.box(row, c)) {
            if (box) return false;
            box = true;
        }
    }
    return true;
}

// empty or one box
bool HasEmptyColX(const LevelEnv& env, int col) {
    const int re = env.wall.rows() - 2;
    bool box = false;
    for (int r = 2; r < re; r++) {
        if (env.wall(r, col)) return false;
        if (env.box(r, col)) {
            if (box) return false;
            box = true;
        }
    }
    return true;
}

bool HasEmptyRowX(const LevelEnv& env) {
    for (int r = 2; r < env.wall.rows() - 2; r++) {
        if (HasEmptyRowX(env, r)) return true;
    }
    return false;
}

bool HasEmptyColX(const LevelEnv& env) {
    for (int c = 2; c < env.wall.cols() - 2; c++) {
        if (HasEmptyColX(env, c)) return true;
    }
    return false;
}

bool Box(const LevelEnv& env, int row, int col) { return env.box(row + 2, col + 2); }
bool Wall(const LevelEnv& env, int row, int col) { return env.wall(row + 2, col + 2); }
bool Empty(const LevelEnv& env, int row, int col) { return !env.wall(row + 2, col + 2) && !env.box(row + 2, col + 2); }

bool HasFreeCornerBox(const LevelEnv& env) {
    const int re = env.wall.rows() - 5;
    const int ce = env.wall.cols() - 5;
    if (Box(env, 0, 0) && (Empty(env, 1, 0) || Empty(env, 0, 1))) return true;
    if (Box(env, re, 0) && (Empty(env, re - 1, 0) || Empty(env, re, 1))) return true;
    if (Box(env, 0, ce) && (Empty(env, 1, ce) || Empty(env, 0, ce - 1))) return true;
    if (Box(env, re, ce) && (Empty(env, re - 1, ce) || Empty(env, re, ce - 1))) return true;
    return false;
}

bool Has2x2Deadlock(const LevelEnv& e) {
    const int rows = e.wall.rows() - 4;
    const int cols = e.wall.cols() - 4;
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            if (Box(e, r, c) || Box(e, r+1, c) || Box(e, r, c+1) || Box(e, r+1, c+1)) {
                if (!Empty(e, r, c) && !Empty(e, r+1, c) && !Empty(e, r, c+1) && !Empty(e, r+1, c+1)) return true;

                if (Box(e, r, c) && Wall(e, r+1, c) && Wall(e, r, c+1)) return true;
                if (Box(e, r+1, c) && Wall(e, r, c) && Wall(e, r+1, c+1)) return true;
                if (Box(e, r, c+1) && Wall(e, r+1, c+1) && Wall(e, r, c)) return true;
                if (Box(e, r+1, c+1) && Wall(e, r, c+1) && Wall(e, r+1, c)) return true;
            }
        }
    }
    return false;
}

void FindAll(int rows, int cols) {
    LevelEnv env = MakeEnv(rows, cols);
    ulong icode_max = std::pow(3, rows * cols) - 1;

    int count = 0;
    vector<char> code(rows * cols, 0);
    ulong icode = 0;
    int num_boxes = 0;
    while (Increment(code, &num_boxes)) {
        icode += 1;
        if (num_boxes == 0) continue;
        Apply(code, &env);
        if (HasEmptyRowX(env) || HasEmptyColX(env)) continue;
        if (HasFreeCornerBox(env)) continue;
        if (Has2x2Deadlock(env)) continue;

        count += 1;
        if (count % 10000 == 0) {
            env.Print(false);
            print("{}\n", double(icode) / icode_max);
        }
    }
    print("{} x {} -> {}\n", rows, cols, count);
}

int main(int argc, char* argv[]) {
    FindAll(2, 4);
    FindAll(3, 3);
    FindAll(2, 5);
    FindAll(3, 4);
    FindAll(4, 4);
    FindAll(3, 5);
    FindAll(4, 5);
    return 0;
}
