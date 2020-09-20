#include "core/timestamp.h"
#include "core/fmt.h"
#include "core/file.h"
#include "core/exception.h"
#include "core/vector.h"
#include "core/matrix.h"
#include "core/murmur3.h"
#include "core/callstack.h"
#include "core/each.h"
#include "core/thread.h"

#include "absl/container/flat_hash_set.h"

#include <thread>
#include <filesystem>
#include <string_view>
#include <fstream>
#include <shared_mutex>

using namespace std::chrono_literals;
using std::string_view;
using std::string;
using std::vector;
using std::array;
using std::pair;
using std::ostream;
using std::cout;
using std::endl;
using std::mutex;
using std::shared_mutex;
using std::shared_lock;
using std::unique_lock;
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
    Level(const int rows, const int cols) { Reset(rows, cols); }
    Level(const vector<string>& lines, bool emoji = false);

    void Reset(const int rows, const int cols) {
        this->rows = rows;
        this->cols = cols;
        cell.resize(1 + rows * cols);

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

    //bool Increment();
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
};

int NumBoxes(const Level& level) {
    int count = 0;
    for (int i = 1; i < level.cell.size(); i++) {
        if (level.cell[i].box) count += 1;
    }
    return count;
}

bool Solver::IsSolveable(Level& level) {
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

    while (queued > 0) {
        if (minimal == 0) return true;
        while (remaining[minimal].empty()) minimal += 1;
        const State s = remaining[minimal].back();
        remaining[minimal].pop_back();
        queued -= 1;
        const int s_num_boxes = minimal;

        for_each_push(level, s.agent, s.boxes, [&](const int a, const int b, const int dir) {
            State ns = s;
            const int c = level[b].dir(dir);
            ns.agent = b;
            ns.boxes.reset(b);
            if (c != 0) ns.boxes.set(c);

            if (c != 0 && is_simple_deadlock(level, c, ns.boxes)) return;
            normalize(level, ns.agent, ns.boxes);
            if (visited.contains(ns)) return;
            visited.insert(ns);
            const int ns_num_boxes = (c == 0) ? s_num_boxes - 1 : s_num_boxes;
            remaining[ns_num_boxes].push_back(ns);
            if (ns_num_boxes < minimal) minimal = ns_num_boxes;
            queued += 1;
        });
    }
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

bool HasWallTetris(const Level& level) {
    for (const Cell& v : level.cell) {
        if (v.wall) {
            int w = 0;
            for (int i = 0; i < 4; i++) {
                if (level.cell[v._dir[i]].wall) w += 1;
            }
            if (w >= 3) return true;
        }
    }
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

vector<Pattern> LoadPatternsFromFile(const string& path) {
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

struct EPatterns {
    EPatterns() {}

    EPatterns(EPatterns&& o) : _count(o._count), _pattern_size(o._pattern_size), _data(std::move(o._data)) {
        o._count = 0;
        o._pattern_size = 0;
    }

    int size() const { return _count; }

    string_view operator[](int i) const {
        return string_view(_data.data() + i * _pattern_size, _pattern_size);
    }

    void add(const string_view a) {
        if (_count == 0) {
            _pattern_size = a.size();
        } else {
            if (a.size() != _pattern_size) THROW(runtime_error, "mismatch");
        }
        _data.insert(_data.end(), a.begin(), a.end());
        _count += 1;
    }

    void add(const vector<char>& a) {
        if (_count == 0) {
            _pattern_size = a.size();
        } else {
            if (a.size() != _pattern_size) THROW(runtime_error, "mismatch");
        }
        _data.insert(_data.end(), a.begin(), a.end());
        _count += 1;
    }

private:
    int _count = 0;
    int _pattern_size = 0;
    vector<char> _data;
};

bool Matches(const string_view pattern, const vector<char>& code) {
    for (int i = 0; i < pattern.size(); i++) {
        if (pattern[i] > code[i]) return false;
    }
    return true;
}

bool ContainsExistingPattern(const EPatterns& patterns, const vector<char>& code) {
    for (int i = 0; i < patterns.size(); i++) {
        if (Matches(patterns[i], code)) return true;
    }
    return false;
}

struct XPattern {
    int rows, cols;
    EPatterns patterns;

    XPattern(int rows, int cols, EPatterns o) : rows(rows), cols(cols), patterns(std::move(o)) {}
};

using XPatterns = vector<XPattern>;

struct each_pair : public each<each_pair> {
        int ma, mb;
        int a = 0, b = -1;

        each_pair(int ma, int mb) : ma(std::max<int>(0, ma)), mb(std::max<int>(0, mb)) {}

        std::optional<pair<int, int>> next() {
            b += 1;
            if (b == mb) {
                b = 0;
                a += 1;
                if (a == ma) return std::nullopt;
            }
            return pair{a, b};
        }
};

bool ContainsExistingPatternCrop(const XPattern& xp, int rows, int cols, const vector<char>& code, vector<char>& crop) {
    crop.resize(xp.rows * xp.cols);
    for (auto [c, r] : each_pair(cols - xp.cols + 1, rows - xp.rows + 1)) {
        // crop [code] to [crop]
        for (auto [ec, er] : each_pair(xp.cols, xp.rows)) {
            crop[er * xp.cols + ec] = code[(r + er) * cols + c + ec];
        }

        if (ContainsExistingPattern(xp.patterns, crop)) return true;
    }
    return false;
}

bool ContainsExistingPattern(const XPatterns& xpatterns, int rows, int cols, const vector<char>& code, vector<char>& crop, vector<char>& transposed) {
    transposed.clear();
    for (const XPattern& xp : xpatterns) {
        if (rows == xp.rows && cols == xp.cols) {
            if (ContainsExistingPattern(xp.patterns, code)) return true;
            continue;
        }

        if (rows >= xp.rows && cols >= xp.cols) {
            if (ContainsExistingPatternCrop(xp, rows, cols, code, crop)) return true;
        }

        if (xp.rows != xp.cols && rows >= xp.cols && cols >= xp.rows) {
            if (transposed.size() != rows * cols) {
                transposed.resize(rows * cols);
                for (auto [r, c] : each_pair(rows, cols)) transposed[c * rows + r] = code[r * cols + c];
            }
            if (ContainsExistingPatternCrop(xp, cols, rows, transposed, crop)) return true;
        }
    }
    return false;
}

void AddPattern(EPatterns& patterns, int rows, int cols, const vector<char>& code) {
    patterns.add(code);

    // Add all transformations of code!
    const int num_patterns = patterns.size();
    const int num_transforms = (rows == cols) ? 8 : 4;
    string cc;
    cc.resize(code.size());

    for (int i = 1; i < num_transforms; i++) {
        for (auto [r, c] : each_pair(rows, cols)) {
            int mr = r, mc = c;
            if (i & 1) mc = cols - 1 - mc;
            if (i & 2) mr = rows - 1 - mr;
            if (i & 4) std::swap(mr, mc);
            cc[r * cols + c] = code[mr * cols + mc];
        }

        bool symmetric = false;
        for (int j = num_patterns; j < patterns.size(); j++) {
            if (cc == patterns[j]) {
                symmetric = true;
                break;
            }
        }
        if (!symmetric) {
            patterns.add(cc);
        }
    }
}

template<int transform>
ulong Encode(const vector<char>& code, const int rows, const int cols) {
    ulong a = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            a *= 3;
            if (transform == 0) a += code[r * cols + c];
            if (transform == 1) a += code[r * cols + (cols - 1 - c)];
            if (transform == 2) a += code[(rows - 1 - r) * cols + c];
            if (transform == 3) a += code[(rows - 1 - r) * cols + (cols - 1 - c)];
            if (transform == 4) a += code[c * cols + r];
            if (transform == 5) a += code[(cols - 1 - c) * cols + r];
            if (transform == 6) a += code[c * cols + (rows - 1 - r)];
            if (transform == 7) a += code[(cols - 1 - c) * cols + (rows - 1 - r)];
        }
    }
    return a;
}

bool IsCanonical(const vector<char>& code, const int rows, const int cols) {
    ulong a = Encode<0>(code, rows, cols);
    if (a > Encode<1>(code, rows, cols)) return false;
    if (a > Encode<2>(code, rows, cols)) return false;
    if (a > Encode<3>(code, rows, cols)) return false;
    if (rows != cols) return true;
    if (a > Encode<4>(code, rows, cols)) return false;
    if (a > Encode<5>(code, rows, cols)) return false;
    if (a > Encode<6>(code, rows, cols)) return false;
    if (a > Encode<7>(code, rows, cols)) return false;
    return true;
}

struct Permutations {
    int rows, cols;
    vector<char> code;
    XPatterns xpatterns;
    std::ofstream of;

    shared_mutex new_patterns_mutex;
    EPatterns new_patterns;

    int Find(int boxes, int walls);
    int Find(int pieces);

    void Run(int rows, int cols);
};

constexpr int Batch = 100;

struct ThreadLocal {
    array<vector<char>, Batch> code;
    Level level;
    vector<char> crop;
    vector<char> transposed;
    Solver solver;
};

int Permutations::Find(const int boxes, const int walls) {
    int w = 0;
    const int spaces = rows * cols - boxes - walls;
    for (int i = 0; i < spaces; i++) code[w++] = 0;
    for (int i = 0; i < boxes; i++) code[w++] = 1;
    for (int i = 0; i < walls; i++) code[w++] = 2;

    mutex print_mutex;
    mutex next_perm_mutex;

    bool running = true;
    int count = 0;

    parallel([&, walls, this]() {
        static thread_local ThreadLocal w;
        if (w.level.rows != rows || w.level.cols != cols) w.level.Reset(rows, cols);

        while (true) {
            int accum = 0;
            {
                unique_lock<mutex> lock(next_perm_mutex);
                if (!running) break;
                for (int i = 0; running && i < Batch; i++) {
                    w.code[i] = code;
                    accum += 1;
                    running = std::next_permutation(code.begin(), code.end());
                }
            }

            for (int batch = 0; batch < accum; batch++) {
                // Convert code to level
                const int num_cells = w.level.cell.size();
                for (int i = 1; i < num_cells; i++) {
                    char c = w.code[batch][i - 1];
                    w.level.cell[i].box = c == 1;
                    w.level.cell[i].wall = c == 2;
                }

                if (walls >= 3 && HasWallCorner(w.level)) continue;
                if (HasFreeCornerBox(w.level)) continue;
                if (walls >= 4 && HasWallTetris(w.level)) continue;
                if (HasEmptyRowX(w.level) || HasEmptyColX(w.level)) continue;
                if (Has2x2Deadlock(w.level)) continue;

                if (!IsCanonical(w.code[batch], rows, cols)) continue;
                if (HasFreeBox(w.level)) continue;

                if (ContainsExistingPattern(xpatterns, rows, cols, w.code[batch], w.crop, w.transposed)) continue;
                if (w.solver.IsSolveable(w.level)) continue;

                {
                    unique_lock lock(new_patterns_mutex);
                    AddPattern(new_patterns, rows, cols, w.code[batch]);
                }

                unique_lock<mutex> lock(print_mutex);
                count += 1;
                w.level.Print();
                w.level.Print(of);
            }
        }
    });

    if (new_patterns.size() > 0) {
        xpatterns.emplace_back(rows, cols, std::move(new_patterns));
    }
    return count;
}

int Permutations::Find(const int pieces) {
    int count = 0;
    for (int boxes = pieces; boxes >= 1; boxes--) {
        count += Find(boxes, pieces - boxes);
    }
    return count;
}

void Permutations::Run(const int rows, const int cols) {
    this->rows = rows;
    this->cols = cols;
    code.resize(rows * cols);
    of = std::ofstream(format("{}/{}x{}.mt", kPatternsPath, rows, cols));

    print("{} x {}\n", rows, cols);

    const int rh = std::max(1, (rows - 1) / 2);
    const int ch = std::max(1, (cols - 1) / 2);
    const int max_pieces = rows * cols - rh * ch;

    Timestamp start;
    int count = 0;
    for (int pieces = 2; pieces <= max_pieces; pieces++) {
        if (rows * cols >= 16) print("{} x {} with {} pieces\n", rows, cols, pieces);
        count += Find(pieces);
    }
    print("{} x {} done! {} patterns, computed in {:.2f} min\n\n", rows, cols, count, start.elapsed_s() / 60);
}

int main(int argc, char* argv[]) {
    InitSegvHandler();
    std::filesystem::create_directories(kPatternsPath);

    Permutations p;
    p.Run(2, 3); // 6
    p.Run(2, 4); // 8
    p.Run(3, 3); // 9
    p.Run(2, 5); // 10
    p.Run(2, 6); // 12
    p.Run(3, 4); // 12
    p.Run(3, 5); // 15
    p.Run(4, 4); // 16
    p.Run(3, 6); // 18
    p.Run(4, 5); // 20
    p.Run(4, 6); // 24
    p.Run(5, 5); // 25
    return 0;
}
