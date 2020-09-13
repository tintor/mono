#include "sokoban/level_env.h"
#include "sokoban/level.h"
#include "sokoban/util.h"
#include "core/fmt.h"
#include "absl/container/flat_hash_set.h"

using std::vector;
using absl::flat_hash_set;

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

// TODO DenseBoxes based on single ulong!
using State = TState<DenseBoxes<1>>;

struct Solved {};

bool IsSolveable(const LevelEnv& env) {
    const Level* level = LoadLevel(env, /*extra*/false);
    State start = level->start;
    normalize(level, start);

    absl::flat_hash_set<State> visited;
    vector<State> remaining;
    visited.insert(start);
    remaining.push_back(start);

    try {
        while (!remaining.empty()) {
            const State s = remaining.back();
            remaining.pop_back();

            for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                State ns = s;
                const Cell* c = b->dir(d);
                if (c == nullptr) THROW(runtime_error, "wtf");
                ns.boxes.reset(b->id);
                if (!c->sink) ns.boxes.set(c->id);

                if (c->sink && ns.boxes == State::Boxes()) throw Solved();
                normalize(level, ns);

                if (visited.contains(ns)) return;
                visited.insert(ns);
                remaining.push_back(ns);
            });
        }
        return false;
    } catch (Solved) {
        return true;
    }
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

bool HasWallCorner(const LevelEnv& e) {
    const int re = e.wall.rows() - 5;
    const int ce = e.wall.cols() - 5;
    if (Wall(e, 0, 0) && Wall(e, 0, 1) && Wall(e, 1, 0)) return true;
    if (Wall(e, re, 0) && Wall(e, re-1, 0) && Wall(e, re, 1)) return true;
    if (Wall(e, 0, ce) && Wall(e, 0, ce-1) && Wall(e, 1, ce)) return true;
    if (Wall(e, re, ce) && Wall(e, re-1, ce) && Wall(e, re, ce-1)) return true;
    return false;
}

bool Has2x2Deadlock(const LevelEnv& e) {
    const int rows = e.wall.rows() - 4;
    const int cols = e.wall.cols() - 4;
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            int boxes = 0;
            if (Box(e, r, c)) boxes += 1;
            if (Box(e, r+1, c)) boxes += 1;
            if (Box(e, r, c+1)) boxes += 1;
            if (Box(e, r+1, c+1)) boxes += 1;
            if (boxes == 0) continue;

            // One box and two diagonal walls
            if (Wall(e, r+1, c) && Wall(e, r, c+1)) return true;
            if (Wall(e, r, c) && Wall(e, r+1, c+1)) return true;

            int walls = 0;
            if (Wall(e, r, c)) walls += 1;
            if (Wall(e, r+1, c)) walls += 1;
            if (Wall(e, r, c+1)) walls += 1;
            if (Wall(e, r+1, c+1)) walls += 1;

            if (walls == 2 && boxes == 2) return true;
            if (walls + boxes == 4) return true;
        }
    }
    return false;
}

bool IsMinimal(const LevelEnv& e, int num_boxes) {
    const int rows = e.wall.rows() - 4;
    const int cols = e.wall.cols() - 4;

    LevelEnv o;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (num_boxes > 1 && Box(e, r, c)) {
                o = e;
                o.box(r + 2, c + 2) = false; // remove box
                if (!IsSolveable(o)) {
                    return false;
                }
                /*o.wall(r + 2, c + 2) = true; // replace box with wall
                if (!Has2x2Deadlock(o) && !IsSolveable(o)) {
                    return false;
                }*/
            }
            if (Wall(e, r, c)) {
                o = e;
                o.wall(r + 2, c + 2) = false; // remove wall
                if (!IsSolveable(o)) {
                    return false;
                }
            }
        }
    }
    return true;
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
        if (num_boxes < 2) continue;
        Apply(code, &env);
        if (HasEmptyRowX(env) || HasEmptyColX(env)) continue;
        if (HasWallCorner(env)) continue;
        if (HasFreeCornerBox(env)) continue;
        if (Has2x2Deadlock(env)) continue;
        // TODO symmetry
        if (IsSolveable(env)) continue;
        if (!IsMinimal(env, num_boxes)) continue;

        count += 1;
        env.Print(false);
        print("progress {}\n", double(icode) / icode_max);
    }
    print("{} x {} -> {}\n", rows, cols, count);
}

int main(int argc, char* argv[]) {
    FindAll(2, 3);
    FindAll(2, 4);
    FindAll(3, 3);
    FindAll(2, 5);
    FindAll(3, 4);
    FindAll(3, 5);
    //FindAll(4, 4);
    //FindAll(4, 5);
    return 0;
}
