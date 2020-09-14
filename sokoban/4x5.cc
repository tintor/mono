#include "sokoban/level_env.h"
#include "sokoban/level.h"
#include "sokoban/util.h"

#include "core/timestamp.h"
#include "core/fmt.h"
#include "core/file.h"

#include "absl/container/flat_hash_set.h"

#include <thread>
#include <filesystem>
#include <string_view>
#include <fstream>

using namespace std::chrono_literals;
using std::string_view;
using std::string;
using std::vector;
using absl::flat_hash_set;

constexpr string_view kPatternsPath = "/tmp/sokoban/patterns";

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

struct Boxes32 {
    using T = ulong;

    bool operator[](uint index) const { return index < 32 && (data | mask(index)) == data; }

    void set(uint index) {
        if (index >= 32) THROW(runtime_error, "out of range");
        data |= mask(index);
    }

    void reset(uint index) {
        if (index >= 32) THROW(runtime_error, "out of range");
        data &= ~mask(index);
    }

    void reset() { data = 0; }

    size_t hash() const { return data; }

    bool operator==(const Boxes32 o) const { return data == o.data; }

    bool contains(const Boxes32 o) const { return (data | o.data) == data; }

   private:
    static T mask(uint index) { return T(1) << index; }

    T data = 0;
};

ulong Encode(const LevelEnv& e) {
    ulong a = 0;
    ulong p = 1;

    const int cols = e.wall.cols() - 4;
    const int rows = e.wall.rows() - 4;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int2 m = {c + 2, r + 2};
            if (e.box(m)) a += p;
            if (e.wall(m)) a += 2 * p;
            p *= 3;
        }
    }
    return a;
}

ulong Encode(const LevelEnv& e, int transform) {
    ulong a = 0;
    ulong p = 1;

    const int cols = e.wall.cols() - 4;
    const int rows = e.wall.rows() - 4;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int2 m = {c, r};
            if (transform & 1) {
                // mirror x
                m = {cols - 1 - m.x, m.y};
            }
            if (transform & 2) {
                // mirror y
                m = {m.x, rows - 1 - m.y};
            }
            if (transform & 4) {
                // transpose
                m = {m.y, m.x};
            }
            m += {2, 2};
            if (e.box(m)) a += p;
            if (e.wall(m)) a += 2 * p;
            p *= 3;
        }
    }
    return a;
}

int Transforms(const LevelEnv& e) {
    return (e.wall.cols() == e.wall.rows()) ? 8 : 4;
}

bool IsCanonical(const LevelEnv& e, ulong icode) {
    const int transforms = Transforms(e);
    for (int i = 0; i < transforms; i++) {
        ulong a = Encode(e, i);
        if (a < icode) return false;
    }
    return true;
}

ulong Canonical(const LevelEnv& e) {
    const int transforms = Transforms(e);
    ulong a = -1;
    for (int i = 0; i < transforms; i++) a = std::min(a, Encode(e, i));
    return a;
}

bool HasFreeBox(const LevelEnv& e) {
    // Compute agent reachability
    matrix<char> reachable;
    reachable.resize(e.wall.rows(), e.wall.cols());
    reachable.fill(false);
    vector<int2> remaining = {{1, 1}};
    reachable(1, 1) = true;
    while (remaining.size() > 0) {
        int2 a = remaining.back();
        remaining.pop_back();
        for (int2 d : {int2{1,0}, {-1,0}, {0,1}, {0,-1}}) {
            int2 b = a + d;
            if (!e.wall(b) && !e.box(b) && !reachable(b)) {
                remaining.push_back(b);
                reachable(b) = true;
            }
        }
    }

    // Look for boxes which are reachable and pushable
    for (int r = 2; r < e.wall.rows() - 2; r++) {
        for (int c = 0; c < e.wall.cols() - 2; c++) {
            int2 b = {c + 2, r + 2};
            if (e.box(b)) {
                for (int2 d : {int2{1,0}, {-1,0}, {0,1}, {0,-1}}) {
                    if (reachable(b - d)) {
                        int2 a = b;
                        while (true) {
                            a += d;
                            if (e.sink(a)) return true;
                            if (e.wall(a) || e.box(a)) break;
                        }
                    }
                }
            }
        }
    }
    return false;
}

using State = TState<Boxes32>;

struct Solved {};

struct Solver {
    bool IsSolveable(const LevelEnv& env, ulong icode);

    flat_hash_set<State> visited;
    vector<State> remaining;

    flat_hash_set<ulong> solveable;
    flat_hash_set<ulong> unsolveable;
};

bool Solver::IsSolveable(const LevelEnv& env, ulong icode) {
    ulong canonical_icode = Canonical(env);
    if (solveable.contains(canonical_icode)) return true;
    if (unsolveable.contains(canonical_icode)) return false;

    const Level* level = LoadLevel(env, /*extra*/false);
    ON_SCOPE_EXIT(Destroy(level));
    if (level->num_alive > 32) THROW(runtime_error, "num_alive > 32");
    State start = level->start;
    normalize(level, start);

    visited.clear();
    remaining.clear();
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

                if (c->sink) {
                    if (ns.boxes == State::Boxes()) throw Solved();
                } else {
                    ns.boxes.set(c->id);
                    if (is_simple_deadlock(c, ns.boxes)) return;
                }

                normalize(level, ns);
                if (visited.contains(ns)) return;
                visited.insert(ns);
                remaining.push_back(ns);
            });
        }
    } catch (Solved) {
        solveable.insert(canonical_icode);
        return true;
    }

    unsolveable.insert(canonical_icode);
    LevelEnv o = env;
    for (const State& s : visited) {
        if (s.agent != start.agent) continue;

        for (int i = 0; i < level->num_alive; i++) {
            int2 m = level->cell_to_vec(level->cells[i]);
            o.box(m) = s.boxes[i];
        }
        unsolveable.insert(Encode(o, 0));
    }
    return false;
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

// Looks for this pattern:
// ???
// #??
// ##?

// TODO also:
// ???
// ..?
// #.?
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

bool IsMinimal(const LevelEnv& e, ulong icode, Solver* solver, int num_boxes) {
    const int rows = e.wall.rows() - 4;
    const int cols = e.wall.cols() - 4;

    LevelEnv o = e;
    ulong p = 1;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (num_boxes > 1 && Box(e, r, c)) {
                o.box(r + 2, c + 2) = false;
                if (!solver->IsSolveable(o, icode - p)) return false;
                o.box(r + 2, c + 2) = true;
            }
            if (Wall(e, r, c)) {
                o.wall(r + 2, c + 2) = false; // remove wall
                if (!solver->IsSolveable(o, icode - p * 2)) return false;
                o.box(r + 2, c + 2) = true; // wall -> box
                if (!solver->IsSolveable(o, icode - p)) return false;
                o.box(r + 2, c + 2) = false;
                o.wall(r + 2, c + 2) = true;
            }
            p *= 3;
        }
    }
    return true;
}

void PrintToFile(std::ofstream& os, const LevelEnv& env) {
    for (int r = 2; r < env.wall.rows() - 2; r++) {
        for (int c = 2; c < env.wall.cols() - 2; c++) {
            if (env.wall(r, c)) os.put('#');
            else if (env.box(r, c)) os.put('$');
            else os.put('.');
        }
        os.put('\n');
    }
    os.put('\n');
    os.flush();
}

struct Pattern {
    int rows, cols;
    vector<char> cells;

    Pattern() {}
    Pattern(const vector<string> lines);
    bool MatchesAt(const LevelEnv& env, int dr, int dc) const;
    bool Matches(const LevelEnv& env) const;
};

Pattern::Pattern(const vector<string> lines) {
    if (lines.size() == 0) THROW(runtime_error, "empty lines");
    rows = lines.size();
    cols = lines[0].size();
    for (const auto& line : lines) {
        if (line.size() != cols) THROW(runtime_error, "uneven lines");
    }

    cells.resize(rows * cols);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            char m = lines[r][c];
            if (m != '.' && m != '$' && m != '#') THROW(runtime_error, "unexpected char: {}", m);
            if (m == '.') m = ' ';
            cells[r * cols + c] = m;
        }
    }
}

char Cell(const LevelEnv& env, int r, int c) {
    if (env.wall(r + 2, c + 2)) return '#';
    if (env.box(r + 2, c + 2)) return '$';
    return ' ';
}

bool Pattern::MatchesAt(const LevelEnv& env, const int dr, const int dc) const {
    int boxes = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const char a = cells[r * cols + c];
            const char b = Cell(env, dr + r, dc + c);
            if (b == '$') boxes += 1;
            if (a == b || a == ' ') continue;
            if (a == '#' && b != '#') return false;
            if (a == '$' && b == '#') continue;
            if (a == '$' && b == ' ') return false;
        }
    }
    return boxes > 0;
}

bool Pattern::Matches(const LevelEnv& env) const {
    const int er = env.wall.rows() - 4;
    const int ec = env.wall.cols() - 4;

    const int mr = er - rows + 1;
    const int mc = ec - cols + 1;
    for (int r = 0; r < mr; r++) {
        for (int c = 0; c < mc; c++) {
            if (MatchesAt(env, r, c)) return true;
        }
    }
    return false;
}

class Patterns {
public:
    bool Matches(const LevelEnv& env) const;
    void Add(const Pattern& pattern);
    void LoadFromDisk();
private:
    vector<Pattern> _data;
};

bool Patterns::Matches(const LevelEnv& env) const {
    for (const Pattern& p : _data) {
        if (p.Matches(env)) return true;
    }
    return false;
}

vector<Pattern> LoadPatternsFromFile(string_view path) {
    vector<Pattern> patterns;
    vector<string> lines;

    string line;
    std::ifstream in(path);
    while (std::getline(in, line)) {
        if (line.empty() && !lines.empty()) {
            patterns.push_back(Pattern(lines));
            lines.clear();
        } else {
            lines.push_back(line);
        }
    }
    return patterns;
}

void Patterns::LoadFromDisk() {
    for (const auto& entry : std::filesystem::directory_iterator(kPatternsPath)) {
        for (const Pattern& p : LoadPatternsFromFile(entry.path().string())) {
            Add(p);
        }
    }
}

void Patterns::Add(const Pattern& pattern) {
    for (int transform = 0; transform < 8; transform++) {
        if (transform >= 4 && pattern.rows != pattern.cols) break;

        Pattern p = pattern;
        // TODO transform p!
        _data.push_back(p);
    }
}

void LoadPatternIntoEnv(LevelEnv& env, const Pattern& p) {
    const int rows = env.wall.rows() - 4;
    const int cols = env.wall.cols() - 4;
    if (p.rows != rows || p.cols != cols) THROW(runtime_error, "pattern size mismatch with env");

    // Load pattern into env
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const char m = p.cells[r * cols + c];
            env.wall(r + 2, c + 2) = m == '#';
            env.box(r + 2, c + 2) = m == '$';
            if (m != ' ' && m != '$' && m != '#') THROW(runtime_error, "unexpected char {}", m);
        }
    }
}

void FindAll(int rows, int cols, bool resume = false) {
    Patterns patterns;
    patterns.LoadFromDisk();

    LevelEnv env = MakeEnv(rows, cols);

    ulong icode_min = 0;
    vector<char> code(rows * cols, 0);

    if (resume) {
        const auto path = format("{}/{}x{}.partial", kPatternsPath, rows, cols);
        const Pattern p = LoadPatternsFromFile(path).back();
        LoadPatternIntoEnv(env, p);
        icode_min = Encode(env);
        // load pattern into code
        for (int i = 0; i < p.cells.size(); i++) {
            if (p.cells[i] == ' ') code[i] = 0;
            if (p.cells[i] == '$') code[i] = 1;
            if (p.cells[i] == '#') code[i] = 2;
        }

        print("starting pattern\n");
        env.Print(false);
        print("\n");
    }

    std::ofstream of(format("{}/{}x{}", kPatternsPath, rows, cols));
    ulong icode_max = std::pow(3, rows * cols) - 1;

    Solver solver;
    int count = 0;
    ulong icode = icode_min;
    int num_boxes = 0;

    Timestamp start_ts;
    std::atomic<bool> running = true;
    std::thread monitor([&]() {
        while (running) {
            for (int i = 0; i < 10; i++) {
                std::this_thread::sleep_for(500ms);
                if (!running) return;
            }

            double progress = double(icode - icode_min) / (icode_max - icode_min);
            print("progress {}", progress);
            print(", icode {}", icode);
            print(", solvable {}", solver.solveable.size());
            print(", unsolveable {}", solver.unsolveable.size());
            print(", patterns {}", count);

            double elapsed = start_ts.elapsed_s();
            double eta = (1 - progress) * elapsed / progress;
            long eta_hours = eta / 3600;
            print(", ETA {}:{}\n", eta_hours, (eta - eta_hours * 3600) / 60);
        }
    });

    while (Increment(code, &num_boxes)) {
        icode += 1;
        if (num_boxes < 2) continue;
        Apply(code, &env);
        if (HasWallCorner(env)) continue;
        if (HasFreeCornerBox(env)) continue;
        if (HasEmptyRowX(env) || HasEmptyColX(env)) continue;
        if (Has2x2Deadlock(env)) continue;
        if (HasFreeBox(env)) continue;
        if (!IsCanonical(env, icode)) continue;
        // if (patterns.Matches(env)) continue;
        if (solver.IsSolveable(env, icode)) continue;
        if (!IsMinimal(env, icode, &solver, num_boxes)) continue;

        count += 1;
        PrintToFile(of, env);
        env.Print(false);
        print("\n");
    }
    running = false;
    monitor.join();
    print("{} x {} -> {}\n", rows, cols, count);
}

// TODO:
// - Parallelize!
// - Change LevelEnv to vector<char> and solve it directly, to avoid building Level structure.
// - Match smaller patters!
// - Match previously found patterns of same size!

int main(int argc, char* argv[]) {
    std::filesystem::create_directories(kPatternsPath);
    /*FindAll(2, 3);
    FindAll(2, 4);
    FindAll(3, 3);
    FindAll(2, 5);
    FindAll(3, 4);
    FindAll(3, 5);
    FindAll(4, 4); // 49s (including all above)*/
    //FindAll(4, 5, true);
    //FindAll(3, 6);
    FindAll(3, 7, true);
    return 0;
}
