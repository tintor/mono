#include "core/timestamp.h"
#include "core/fmt.h"
#include "core/file.h"
#include "core/exception.h"
#include "core/vector.h"
#include "core/matrix.h"
#include "core/murmur3.h"
#include "core/callstack.h"

#include "absl/container/flat_hash_set.h"

#include <thread>
#include <filesystem>
#include <string_view>
#include <fstream>

using namespace std::chrono_literals;
using std::string_view;
using std::string;
using std::vector;
using std::array;
using std::pair;
using std::ostream;
using std::cout;
using std::endl;
using absl::flat_hash_set;

constexpr string_view kPatternsPath = "/tmp/sokoban/patterns";

constexpr int MaxDim = 8;
constexpr int MaxCells = 1 + 5 * 5;

struct Boxes {
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

    bool operator==(const Boxes o) const { return data == o.data; }

    bool contains(const Boxes o) const { return (data | o.data) == data; }

   private:
    static T mask(uint index) { return T(1) << index; }

    T data = 0;
};

struct Cell {
    bool box = false;
    bool wall = false;
    bool alive = false;

    // array of cell_ids
    array<int, 4> _dir;
    int dir(const int d) const { return _dir[d & 3]; }

    // pair of (dir, cell_id)
    static_vector<pair<int, int>, 4 * MaxDim> moves;
};

struct State {
    Boxes boxes;
    int agent;
};

struct Level {
    int rows, cols; // inner dimmensions
    int num_boxes = 0;
    static_vector<Cell, MaxCells> cell; // inner cells + one 0 cell to represent all sinks

    Level() {}
    Level(const int rows, const int cols) : rows(rows), cols(cols), cell(1 + rows * cols) {
        for (int d = 0; d < 4; d++) {
            cell[0]._dir[d] = 0;
        }
        for (int a = 1; a < cell.size(); a++) {
            const int x = (a - 1) % cols;
            const int y = (a - 1) / cols;
            for (int d = 0; d < 4; d++) {
                int xx = x, yy = y;
                if (d == 0) xx -= 1;
                if (d == 1) yy += 1;
                if (d == 2) xx += 1;
                if (d == 3) yy -= 1;
                cell[a]._dir[d] = (0 <= xx && xx < cols && 0 <= yy && yy < rows) ? at(xx, yy) : 0;
            }
        }
    }
    Level(const vector<string>& lines, bool emoji = false);

    bool Increment();
    void Prepare();
    void Print(ostream& os = cout) const;
    void Print(const State& s) const;

    Boxes GetBoxes() const;

    int at(const int x, const int y) const {
        if (x < 0 || x >= cols || y < 0 || y >= rows) THROW(runtime_error, "at(x, y)");
        return 1 + y * cols + x;
    }
    const Cell& operator[](const int i) const {
        if (i < 0 || i >= cell.size()) THROW(runtime_error, "operator[] {}", i);
        return cell[i];
    }
    const Cell& operator()(const int x, const int y) const { return cell[at(x, y)]; }
    Cell& operator()(const int x, const int y) { return cell[at(x, y)]; }
};

constexpr string_view kAgent = "😀";
constexpr string_view kEmpty = "  ";
constexpr string_view kBox = "🔴";
constexpr string_view kWall = "✴️ ";

Level::Level(const vector<string>& lines, bool emoji) {
    if (lines.size() == 0) THROW(runtime_error, "empty lines");
    for (const string& line : lines) {
        if (line.size() != lines[0].size()) THROW(runtime_error, "uneven line");
    }
    rows = lines.size();

    if (emoji) {
        if (lines[0].size() % 2 != 0) THROW(runtime_error, "bad line {}", lines[0].size());
        cols = lines[0].size() / 2;
        cell.resize(1 + rows * cols);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                Cell& v = cell[at(c, r)];
                string_view m(lines[r].data() + c * 2, 2);
                if (m == kEmpty) continue;
                if (m == kWall) { v.wall = true; continue; }
                if (m == kBox) { v.box = true; continue; }
                THROW(runtime_error, "unexpected cell: {}", m);
            }
        }
    } else {
        cols = lines[0].size();
        cell.resize(1 + rows * cols);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                Cell& v = cell[at(c, r)];
                char m = lines[r][c];
                if (m == ' ') continue;
                if (m == '#') { v.wall = true; continue; }
                if (m == '$') { v.box = true; continue; }
                THROW(runtime_error, "unexpected cell: {}", m);
            }
        }
    }
}

void Level::Print(ostream& os) const {
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const Cell& v = cell[at(c, r)];
            if (v.wall) os << kWall;
            if (v.box) os << kBox;
            if (!v.wall && !v.box) os << kEmpty;
        }
        os << '\n';
    }
    os << '\n';
    os.flush();
}

void Level::Print(const State& s) const {
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int id = at(c, r);
            if (s.agent == id) { cout << kAgent; continue; }
            if (s.boxes[id]) { cout << kBox; continue; }
            if (cell[id].wall) { cout << kWall; continue; }
            cout << kEmpty;
        }
        cout << endl;
    }
    cout << endl;
}

Boxes Level::GetBoxes() const {
    Boxes b;
    for (size_t i = 1; i < cell.size(); i++) {
        if (cell[i].box) b.set(i);
    }
    return b;
}

bool Level::Increment() {
    for (size_t i = 1; i < cell.size(); i++) {
        Cell& c = cell[i];

        if (c.box) {
            c.box = false;
            c.wall = true;
            num_boxes -= 1;
            return true;
        }

        if (c.wall) {
            c.wall = false;
            continue;
        }

        c.wall = false;
        c.box = true;
        num_boxes += 1;
        return true;
    }
    return false;
}

void Level::Prepare() {
    // Init moves
    cell[0].moves.clear();
    for (int i = 0; i < cols; i++) {
        const int a = at(i, 0);
        if (!cell[a].wall) cell[0].moves.emplace_back(1, a);
        const int b = at(i, rows - 1);
        if (!cell[b].wall) cell[0].moves.emplace_back(3, b);
    }
    for (int i = 0; i < rows; i++) {
        const int a = at(0, i);
        if (!cell[a].wall) cell[0].moves.emplace_back(2, a);
        const int b = at(cols - 1, i);
        if (!cell[b].wall) cell[0].moves.emplace_back(0, b);
    }
    for (int a = 1; a < cell.size(); a++) {
        Cell& v = cell[a];
        v.moves.clear();
        for (int d = 0; d < 4; d++) {
            if (!cell[v.dir(d)].wall) v.moves.emplace_back(d, v.dir(d));
        }
    }

    // Init alive
    cell[0].alive = true;
    for (int a = 1; a < cell.size(); a++) {
        cell[a].alive = true;
        for (int d = 0; d < 4; d++) {
            if (cell[cell[a].dir(d)].wall && cell[cell[a].dir(d + 1)].wall) {
                cell[a].alive = false;
                break;
            }
        }
    }
}

ulong Encode(const Level& e) {
    ulong a = 0;
    ulong p = 1;

    for (int r = 0; r < e.rows; r++) {
        for (int c = 0; c < e.cols; c++) {
            if (e(c, r).wall) a += p * 2;
            if (e(c, r).box) a += p;
            p *= 3;
        }
    }
    return a;
}

ulong Encode(const Level& e, int transform) {
    ulong a = 0;
    ulong p = 1;

    for (int r = 0; r < e.rows; r++) {
        for (int c = 0; c < e.cols; c++) {
            int mr = r, mc = c;
            // TODO move these IFs outside of loop
            if (transform & 1) {
                // mirror x
                mc = e.cols - 1 - mc;
            }
            if (transform & 2) {
                // mirror y
                mr = e.rows - 1 - mr;
            }
            if (transform & 4) {
                // transpose
                std::swap(mr, mc);
            }
            if (e(mc, mr).wall) a += p * 2;
            if (e(mc, mr).box) a += p;
            p *= 3;
        }
    }
    return a;
}

int Transforms(const Level& e) {
    return (e.cols == e.rows) ? 8 : 4;
}

bool IsCanonical(const Level& e, ulong icode) {
    const int transforms = Transforms(e);
    for (int i = 1; i < transforms; i++) {
        ulong a = Encode(e, i);
        if (a < icode) return false;
    }
    return true;
}

ulong Canonical(const Level& e) {
    const int transforms = Transforms(e);
    ulong a = -1;
    for (int i = 0; i < transforms; i++) a = std::min(a, Encode(e, i));
    return a;
}

class AgentVisitor : public each<AgentVisitor> {
private:
    static_vector<int, MaxCells> _queue;
    array<char, MaxCells> _visited;
    int _head = 0;
    int _tail = 0;

public:
    AgentVisitor() { clear(); }

    bool visited(const int cell_id) const { return _visited[cell_id]; }

    void clear() {
        _head = _tail = 0;
        std::fill(_visited.begin(), _visited.end(), 0);
    }

    bool add(const int cell_id) {
        if (_visited[cell_id]) return false;
        _queue[_tail++] = cell_id;
        _visited[cell_id] = 1;
        return true;
    }

    std::optional<int> next() {
        if (_head == _tail) return std::nullopt;
        return _queue[_head++];
    }
};

bool HasFreeBox(Level& level) {
    level.Prepare();
    AgentVisitor reachable;
    reachable.add(0);
    for (int a : reachable) {
        for (auto [_, b] : level[a].moves) {
            if (!level[b].wall && !level[b].box) reachable.add(b);
        }
    }

    // Look for boxes which are reachable and pushable
    for (int b = 1; b < level.cell.size(); b++) {
        if (level[b].box) {
            for (int d = 0; d < 4; d++) {
                if (reachable.visited(level[b].dir(d ^ 2))) {
                    int a = b;
                    while (true) {
                        a = level[a].dir(d);
                        if (a == 0) return true;
                        if (level[b].box || level[b].wall) break;
                    }
                }
            }
        }
    }
    return false;
}


void normalize(const Level& level, int& agent, const Boxes& boxes) {
    AgentVisitor visitor;
    visitor.add(agent);
    for (const int a : visitor) {
        if (a < agent) {
            agent = a;
            if (a == 0) break;
        }
        for (auto [_, b] : level[a].moves) {
            if (!boxes[b]) visitor.add(b);
        }
    }
}

template <typename Push>
void for_each_push(const Level& level, const int agent, const Boxes& boxes, const Push& push) {
    AgentVisitor visitor;
    visitor.add(agent);
    for (const int a : visitor) {
        for (auto [d, b] : level[a].moves) {
            if (!boxes[b]) {
                visitor.add(b);
                continue;
            }
            const int c = level[b].dir(d);
            if (!level[c].wall && level[c].alive && !boxes[c]) push(a, b, d);
        }
    }
}

bool is_simple_deadlock(const Level& level, const int box, const Boxes& boxes) {
    auto free = [&](const int cell) { return !level[cell].wall && !boxes[cell]; };
    const Cell& v = level[box];

    for (int d = 0; d < 4; d++) {
        // 2x2 deadlock (no need to check wall corner case)
        const int a = v.dir(d);
        const int b = v.dir(d + 1);
        if (!free(a) && !free(b) && !free(level[b].dir(d))) return true;

        // 2x3 deadlock
        if (boxes[a]) {
            // Both A and B are boxes.
            if (!v.dir(d - 1) && !v.dir(d + 1)) return true;
            if (!v.dir(d + 1) && !v.dir(d - 1)) return true;
        }
    }
    return false;
}

bool operator==(const State& a, const State& b) {
    return a.agent == b.agent && a.boxes == b.boxes;
}

namespace std {
template<>
struct hash<State> {
    size_t operator()(const State& a) const { return a.boxes.hash() ^ fmix64(a.agent); }
};
}  // namespace std

struct Solver {
    bool IsSolveable(Level& level);

    flat_hash_set<State> visited;
    vector<vector<State>> remaining;

    // Canonical only.
    flat_hash_set<ulong> solveable;
    flat_hash_set<ulong> unsolveable;

    // Includes non-canonical.
    flat_hash_set<State> known_deadlocks;
};

int NumBoxes(const Level& level) {
    int count = 0;
    for (int i = 1; i < level.cell.size(); i++) {
        if (level.cell[i].box) count += 1;
    }
    return count;
}

bool Solver::IsSolveable(Level& level) {
    ulong canonical_icode = Canonical(level);
    if (solveable.contains(canonical_icode)) return true;
    if (unsolveable.contains(canonical_icode)) return false;

    level.Prepare();
    State start{level.GetBoxes(), 0};
    normalize(level, start.agent, start.boxes);

    visited.clear();
    remaining.resize(level.rows * level.cols + 1);
    for (auto& q : remaining) q.clear();
    visited.insert(start);
    int minimal = NumBoxes(level);
    remaining[minimal].push_back(start);
    size_t queued = 1;

    //print("solving\n");
    while (minimal > 0 && queued > 0) {
        while (remaining[minimal].empty()) minimal += 1;
        const State s = remaining[minimal].back();
        remaining[minimal].pop_back();
        queued -= 1;
        const int s_num_boxes = minimal;

        //print("pop {}\n", s.boxes.data);
        //level.Print(s);

        for_each_push(level, s.agent, s.boxes, [&](const int a, const int b, const int dir) {
            State ns = s;
            const int c = level[b].dir(dir);
            ns.agent = b;
            ns.boxes.reset(b);
            if (c != 0) ns.boxes.set(c);

            //print("  -> {}\n", ns.boxes.data);
            //level.Print(ns);

            if (c != 0 && is_simple_deadlock(level, c, ns.boxes)) return;
            normalize(level, ns.agent, ns.boxes);
            if (visited.contains(ns) /*|| known_deadlocks.contains(ns)*/) return;
            visited.insert(ns);
            const int ns_num_boxes = (c == 0) ? s_num_boxes - 1 : s_num_boxes;
            remaining[ns_num_boxes].push_back(ns);
            if (ns_num_boxes < minimal) minimal = ns_num_boxes;
            queued += 1;
        });
    }

    if (minimal == 0) {
        //print("solved!\n");
        solveable.insert(canonical_icode);
        return true;
    }

    //print("no solution!\n");
    unsolveable.insert(canonical_icode);
    //for (const State& s : visited) known_deadlocks.insert(s);
    return false;
}

// empty or one box
bool HasEmptyRowX(const Level& level, int row) {
    bool one_box = false;
    for (int x = 0; x < level.cols; x++) {
        if (level(x, row).wall) return false;
        if (level(x, row).box) {
            if (one_box) return false;
            one_box = true;
        }
    }
    return true;
}

// empty or one box
bool HasEmptyColX(const Level& level, int col) {
    bool one_box = false;
    for (int y = 0; y < level.rows; y++) {
        if (level(col, y).wall) return false;
        if (level(col, y).box) {
            if (one_box) return false;
            one_box = true;
        }
    }
    return true;
}

bool HasEmptyRowX(const Level& level) {
    for (int y = 0; y < level.rows; y++) {
        if (HasEmptyRowX(level, y)) return true;
    }
    return false;
}

bool HasEmptyColX(const Level& level) {
    for (int x = 0; x < level.cols; x++) {
        if (HasEmptyColX(level, x)) return true;
    }
    return false;
}

bool Box(const Level& level, int row, int col) { return level(col, row).box; }
bool Wall(const Level& level, int row, int col) { return level(col, row).wall; }
bool Empty(const Level& level, int row, int col) { return !level(col, row).wall && !level(col, row).box; }

bool HasFreeCornerBox(const Level& e) {
    const int re = e.rows - 1;
    const int ce = e.cols - 1;
    if (Box(e, 0, 0) && (Empty(e, 1, 0) || Empty(e, 0, 1))) return true;
    if (Box(e, re, 0) && (Empty(e, re - 1, 0) || Empty(e, re, 1))) return true;
    if (Box(e, 0, ce) && (Empty(e, 1, ce) || Empty(e, 0, ce - 1))) return true;
    if (Box(e, re, ce) && (Empty(e, re - 1, ce) || Empty(e, re, ce - 1))) return true;
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
bool HasWallCorner(const Level& e) {
    const int re = e.rows - 1;
    const int ce = e.cols - 1;
    if (Wall(e, 0, 0) && Wall(e, 0, 1) && Wall(e, 1, 0)) return true;
    if (Wall(e, re, 0) && Wall(e, re-1, 0) && Wall(e, re, 1)) return true;
    if (Wall(e, 0, ce) && Wall(e, 0, ce-1) && Wall(e, 1, ce)) return true;
    if (Wall(e, re, ce) && Wall(e, re-1, ce) && Wall(e, re, ce-1)) return true;
    return false;
}

bool Has2x2Deadlock(const Level& e) {
    for (int r = 0; r < e.rows - 1; r++) {
        for (int c = 0; c < e.cols - 1; c++) {
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

bool IsMinimal(Level& level, Solver* solver) {
    for (Cell& v : level.cell) {
        if (v.box) {
            v.box = false;
            bool m = solver->IsSolveable(level);
            v.box = true;
            if (!m) return false;
        }
        if (v.wall) {
            v.wall = false;
            bool m = solver->IsSolveable(level);
            v.wall = true;
            if (!m) return false;

            v.box = true;
            v.wall = false;
            m = solver->IsSolveable(level);
            v.box = false;
            v.wall = true;
            if (!m) return false;
        }
    }
    return true;
}

struct Pattern {
    int rows, cols;
    vector<char> cells;

    Pattern() {}
    Pattern(const vector<string>& lines);
    bool MatchesAt(const Level& level, int dr, int dc) const;
    bool Matches(const Level& level) const;
};

Pattern::Pattern(const vector<string>& lines) {
    if (lines.size() == 0) THROW(runtime_error, "empty lines");
    if (lines[0].size() % 2 != 0) THROW(runtime_error, "bad line");

    for (const string& line : lines) {
        if (line.size() != lines[0].size()) THROW(runtime_error, "uneven line");
    }

    rows = lines.size();
    cols = lines[0].size() / 2;
    for (const auto& line : lines) {
        if (line.size() != cols) THROW(runtime_error, "uneven lines");
    }

    cells.resize(rows * cols);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            string_view m(lines[r].data() + c * 2, 2);
            if (m == kEmpty) { cells[r * cols + c] = ' '; continue; }
            if (m == kWall) { cells[r * cols + c] = '#'; continue; }
            if (m == kBox) { cells[r * cols + c] = '$'; continue; }
            THROW(runtime_error, "unexpected cell: {}", m);
        }
    }
}

char Cell(const Level& level, int r, int c) {
    if (level(c, r).wall) return '#';
    if (level(c, r).box) return '$';
    return ' ';
}

bool Pattern::MatchesAt(const Level& level, const int dr, const int dc) const {
    int boxes = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const char a = cells[r * cols + c];
            const char b = Cell(level, dr + r, dc + c);
            if (b == '$') boxes += 1;
            if (a == b || a == ' ') continue;
            if (a == '#' && b != '#') return false;
            if (a == '$' && b == '#') continue;
            if (a == '$' && b == ' ') return false;
        }
    }
    return boxes > 0;
}

bool Pattern::Matches(const Level& level) const {
    const int er = level.rows;
    const int ec = level.cols;

    const int mr = er - rows + 1;
    const int mc = ec - cols + 1;
    for (int r = 0; r < mr; r++) {
        for (int c = 0; c < mc; c++) {
            if (MatchesAt(level, r, c)) return true;
        }
    }
    return false;
}

class Patterns {
public:
    bool Matches(const Level& level) const;
    void Add(const Pattern& pattern);
    void LoadFromDisk();
private:
    vector<Pattern> _data;
};

bool Patterns::Matches(const Level& level) const {
    for (const Pattern& p : _data) {
        if (p.Matches(level)) return true;
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

void LoadPattern(Level& level, const Pattern& p) {
    if (p.rows != level.rows || p.cols != level.cols) THROW(runtime_error, "pattern size mismatch with env");

    // Load pattern into env
    for (int r = 0; r < level.rows; r++) {
        for (int c = 0; c < level.cols; c++) {
            const char m = p.cells[r * level.cols + c];
            level(c, r).wall = m == '#';
            level(c, r).box = m == '$';
            if (m != ' ' && m != '$' && m != '#') THROW(runtime_error, "unexpected char {}", m);
        }
    }
}

Level g_level;
Solver g_solver;
int g_count;

void Backtrack(int p) {
    if (p == g_level.cell.size()) return;

    // SPACE
    Backtrack(p + 1);

    // BOX
    bool deadlock_with_box = false;
    g_level.cell[p].box = true;
    g_level.num_boxes += 1;
    if (g_level.num_boxes == 0 || g_solver.IsSolveable(g_level)) {
        if (!Has2x2Deadlock(g_level)) Backtrack(p + 1);
    } else {
        deadlock_with_box = true;
        if (g_level.num_boxes >= 2 && !HasFreeCornerBox(g_level) && !HasEmptyRowX(g_level) && !HasEmptyColX(g_level) && IsCanonical(g_level, Encode(g_level)) && IsMinimal(g_level, &g_solver)) {
            g_level.Print();
            g_count += 1;
        }
    }
    g_level.num_boxes -= 1;
    g_level.cell[p].box = false;
    if (deadlock_with_box) return;  // it will also be deadlock with wall

    // WALL
    g_level.cell[p].wall = true;
    if (g_level.num_boxes == 0 || g_solver.IsSolveable(g_level)) {
        if (!Has2x2Deadlock(g_level)) Backtrack(p + 1);
    } else {
        if (g_level.num_boxes >= 2 && !HasFreeCornerBox(g_level) && !HasEmptyRowX(g_level) && !HasEmptyColX(g_level) && IsCanonical(g_level, Encode(g_level)) && IsMinimal(g_level, &g_solver)) {
            g_level.Print();
            g_count += 1;
        }
    }
    g_level.cell[p].wall = false;
}

void FindAll2(int rows, int cols) {
    g_level = Level(rows, cols);
    g_solver = Solver();
    g_count = 0;
    Backtrack(1);
}

void FindAll(int rows, int cols) {
    //Patterns patterns;
    //patterns.LoadFromDisk();

    std::ofstream of(format("{}/{}x{}.new3", kPatternsPath, rows, cols));
    ulong icode_max = std::pow(3, rows * cols) - 1;
    ulong icode = 0;

    Level level(rows, cols);
    Solver solver;
    int count = 0;

    Timestamp start_ts;
    std::atomic<bool> running = true;
    std::thread monitor([&]() {
        while (running) {
            for (int i = 0; i < 10; i++) {
                std::this_thread::sleep_for(500ms);
                if (!running) return;
            }

            double progress = double(icode) / icode_max;
            print("progress {}", progress);
            print(", icode {}", icode);
            print(", solvable {}", solver.solveable.size());
            print(", unsolveable {}", solver.unsolveable.size());
            print(", known_deadlocks {}", solver.known_deadlocks.size());
            print(", patterns {}", count);

            double elapsed = start_ts.elapsed_s();
            double eta = (1 - progress) * elapsed / progress;
            long eta_hours = eta / 3600;
            long elapsed_hours = elapsed / 3600;
            print(", elapsed {}:{}", elapsed_hours, (elapsed - elapsed_hours * 3600) / 60);
            print(", ETA {}:{}\n", eta_hours, (eta - eta_hours * 3600) / 60);
        }
    });

    while (level.Increment()) {
        icode += 1;
        if (level.num_boxes < 2) continue;
        if (HasWallCorner(level)) continue;
        if (HasFreeCornerBox(level)) continue;
        if (HasEmptyRowX(level) || HasEmptyColX(level)) continue;
        if (Has2x2Deadlock(level)) continue;

        if (!IsCanonical(level, icode)) continue;
        if (HasFreeBox(level)) continue;
        // if (patterns.Matches(env)) continue;
        if (solver.IsSolveable(level)) continue;
        if (!IsMinimal(level, &solver)) continue;

        count += 1;
        level.Print(of);
        level.Print();
    }

    running = false;
    monitor.join();
    print("{} x {} -> {}\n", rows, cols, count);
}

// Solving 5x5:
// - all 32 cores on carbide
//   - for ever outer pattern schedule a task in thread pool
// - backtracking: outside then inside
//   - only add box/wall to space (never remove)
//   - only add box/wall if current level is solveable
// - checking smaller patterns

// level can either be:
// - solveable
// - minimal deadlock
// - non-minimal deadlock -> contains smaller patttern

int main(int argc, char* argv[]) {
    InitSegvHandler();

    std::filesystem::create_directories(kPatternsPath);
    /*FindAll(2, 3);
    FindAll(2, 4);
    FindAll(3, 3);
    FindAll(2, 5);
    FindAll(3, 4);
    FindAll(3, 5);
    FindAll(4, 4); // old 9s (including all above)
    FindAll(3, 6);*/ // old 1m3s (including all above)
    FindAll(4, 5); // 7m39s
    //FindAll(3, 7);
    //FindAll(3, 8);
    //FindAll(5, 5);
    return 0;
}
