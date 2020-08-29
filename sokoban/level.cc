#include <unordered_map>

#include "sokoban/level.h"

#include "core/array_deque.h"
#include "core/range.h"
#include "core/small_bfs.h"
#include "core/string.h"
#include "sokoban/util.h"
#include "sokoban/level_env.h"

using std::string_view;
using std::string;
using std::vector;

namespace Code {
constexpr char Wall = '#';
constexpr char Goal = '.';
constexpr char Space = ' ';
constexpr char Dead = ':';
};

struct Minimal {
    // xy is encoded as x + y * W
    int w, h;
    int agent = 0;
    vector<bool> box;
    vector<char> cell;  // Space, Wall or Goal (and later Dead)
    std::array<int, 4> dirs;
    std::array<int, 8> dirs8;
    int cell_count = 0;

    void init(const LevelEnv& env) {
        if (!env.IsValid()) THROW(invalid_argument, "level must be valid");
        w = env.wall.cols();
        h = env.wall.rows();

        box.resize(h * w, false);
        cell.resize(h * w, Code::Space);

        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                int xy = col + row * w;
                int2 i = {col, row};
                if (env.box(i)) box[xy] = true;
                if (env.goal(i)) cell[xy] = Code::Goal;
                if (env.wall(i)) cell[xy] = Code::Wall;
            }
        }
        agent = env.agent.x + env.agent.y * w;

        dirs[0] = -1;
        dirs[1] = +w;
        dirs[2] = +1;
        dirs[3] = -w;

        dirs8[0] = -1;
        dirs8[1] = +w;
        dirs8[2] = +1;
        dirs8[3] = -w;
        dirs8[4] = -1 - w;
        dirs8[5] = +1 - w;
        dirs8[6] = -1 + w;
        dirs8[7] = +1 + w;
    }

    int move_count(int xy) {
        int count = 0;
        for (int m : dirs)
            if (open(xy + m)) count += 1;
        return count;
    }

    int first_move(int xy) {
        for (int m : dirs)
            if (open(xy + m)) return m;
        THROW(runtime_error);
    }

    void check(int xy) const {
        if (xy < 0 || xy >= cell.size()) THROW(runtime_error, "index");
    }
    bool open(int xy) const {
        check(xy);
        return cell[xy] != Code::Wall && cell[xy] != 'e';
    }
    bool empty(int xy) const {
        check(xy);
        return cell[xy] == Code::Space;
    }
    bool goal(int xy) const {
        check(xy);
        return cell[xy] == Code::Goal;
    }
    bool alive(int xy) const {
        check(xy);
        return empty(xy) || goal(xy);
    }

    void move_agent_from_deadend() {
        while (empty(agent) && move_count(agent) == 1) {
            int m = first_move(agent);
            if (box[agent + m]) {
                if (!open(agent + m + m)) break;
                box[agent + m] = false;
                box[agent + m + m] = true;
            }
            cell[agent] = Code::Wall;
            agent += m;
        }
    }

    void remove_deadends() {
        for (int i = 0; i < cell.size(); i++) {
            int a = i;
            while (a >= w && a < cell.size() - w && a != agent && empty(a) && move_count(a) == 1 && !box[a]) {
                int m = first_move(a);
                cell[a] = Code::Wall;
                a += m;
            }
        }
    }

    void cleanup_walls() {
        small_bfs<int> visitor(cell.size());
        visitor.add(agent, agent);
        for (int a : visitor)
            for (int m : dirs)
                if (open(a + m)) visitor.add(a + m, a + m);
        // remove all walls
        for (int i = 0; i < cell.size(); i++)
            if (!visitor.visited[i]) cell[i] = 'e';
        // put minimal walls
        for (int i = 0; i < cell.size(); i++)
            if (visitor.visited[i])
                for (int m : dirs8)
                    if (!visitor.visited[i + m]) cell[i + m] = Code::Wall;
        // compute cell_count
        cell_count = visitor.queue.tail();
    }

    int num_boxes() const {
        int count = 0;
        for (int i = 0; i < box.size(); i++)
            if (box[i]) count += 1;
        return count;
    }

    int num_goals() const {
        int count = 0;
        for (int i = 0; i < box.size(); i++)
            if (goal(i)) count += 1;
        return count;
    }

    uint find_dead_cells() {
        small_bfs<std::pair<ushort, ushort>> visitor(cell.size() * cell.size());
        const auto add = [this, &visitor](uint agent, uint box) {
            check(agent);
            check(box);
            visitor.add(std::pair<ushort, ushort>(agent, box), agent * cell.size() + box);
        };

        vector<char> live(cell.size(), false);
        for (int i = 0; i < cell.size(); i++) {
            if (!goal(i)) continue;
            for (int m : dirs) if (open(i + m)) add(i + m, i);
            live[i] = true;
        }

        for (auto [agent, box] : visitor) {
            for (int m : dirs) {
                if (!open(agent + m)) continue;
                if (agent + m != box) add(agent + m, box);
                if (agent - m != box) continue;
                live[agent] = true;
                add(agent + m, agent);
            }
        }

        uint count = 0;
        for (int i = 0; i < cell.size(); i++) {
            if (live[i] || !open(i)) continue;
            cell[i] = Code::Dead;
            count += 1;
        }
        return count;
    }

    vector<Cell*> cells(Level* level) const {
        std::unordered_map<uint, Cell*> reverse;
        vector<Cell*> cells;
        cells.reserve(cell_count);
        small_bfs<uint> visitor(cell.size());
        visitor.add(agent, agent);

        for (int a : visitor) {
            Cell* c = new Cell;
            c->xy = a;
            c->goal = goal(a);
            c->sink = false;
            c->alive = alive(a);
            c->level = level;
            reverse[a] = c;
            cells.push_back(c);

            for (int m : dirs)
                if (open(a + m)) visitor.add(a + m, a + m);
        }
        sort(cells, [](Cell* a, Cell* b) {
            if (a->goal && !b->goal) return true;
            if (!a->goal && b->goal) return false;
            if (a->alive && !b->alive) return true;
            if (!a->alive && b->alive) return false;
            return a->id < b->id;
        });

        uint id = 0;
        for (Cell* c : cells) c->id = id++;

        for (Cell* c : cells) {
            for (int d = 0; d < 4; d++) c->_dir[d] = open(c->xy + dirs[d]) ? reverse[c->xy + dirs[d]] : nullptr;

            for (int d = 0; d < 8; d++) c->dir8[d] = open(c->xy + dirs8[d]) ? reverse[c->xy + dirs8[d]] : nullptr;

            int m = 0;
            for (int d = 0; d < 4; d++)
                if (c->dir(d)) m += 1;

            c->moves.resize(m);
            m = 0;
            for (int d = 0; d < 4; d++)
                if (c->dir(d)) c->moves[m++] = std::pair<int, Cell*>(d, c->dir(d));

            int p = 0;
            for (int d = 0; d < 4; d++)
                if (c->dir(d) && c->dir(d)->alive && c->dir(d ^ 2)) p += 1;
            c->pushes.resize(p);
            p = 0;
            for (int d = 0; d < 4; d++)
                if (c->dir(d) && c->dir(d)->alive && c->dir(d ^ 2))
                    c->pushes[p++] = std::pair<Cell*, Cell*>(c->dir(d), c->dir(d ^ 2));
        }
        return cells;
    }
};

uint CellCount(string_view name) {
    LevelEnv env;
    env.Load(name);
    Minimal m;
    m.init(env);
    m.move_agent_from_deadend();
    m.remove_deadends();
    m.cleanup_walls();
    return m.cell_count;
}

class PairVisitor : public each<PairVisitor> {
   public:
    PairVisitor(uint size1, uint size2) { visited.resize(size1, size2); }

    bool try_add(uint a, uint b) {
        if (visited(a, b)) return false;
        visited(a, b) = true;
        deque.push_back({a, b});
        return true;
    }

    void reset() {
        deque.clear();
        visited.fill(false);
    }

    std::optional<std::pair<uint, uint>> next() {
        if (deque.empty()) return std::nullopt;
        ON_SCOPE_EXIT(deque.pop_front());
        return deque.front();
    }

   private:
    mt::array_deque<std::pair<uint, uint>> deque;
    matrix<bool> visited;
};

void ComputePushDistances(Level* level) {
    for (Cell* c : level->cells)
        if (c->alive) c->push_distance.resize(level->num_goals, Cell::Inf);

    matrix<uint> distance;
    distance.resize(level->cells.size(), level->num_alive);
    PairVisitor visitor(level->cells.size(), level->num_alive);

    for (Cell* g : level->goals()) {
        auto goal = g->id;
        visitor.reset();
        distance.fill(Cell::Inf);
        for (auto [_, e] : g->moves)
            if (visitor.try_add(e->id, goal)) distance(e->id, goal) = 0;
        g->push_distance[goal] = 0;

        for (auto [agent, box] : visitor) {
            Cell* a = level->cells[agent];
            minimize(level->cells[box]->push_distance[goal], distance(agent, box));

            for (auto [d, n] : a->moves) {
                uint next = n->id;
                if (next != box && visitor.try_add(next, box))
                    distance(next, box) = distance(agent, box);  // no move cost
                if (a->alive && a->dir(d ^ 2) && a->dir(d ^ 2)->id == box && visitor.try_add(next, agent))
                    distance(next, agent) = distance(agent, box) + 1;  // push cost
            }
        }
    }

    for (Cell* b : level->alive()) b->min_push_distance = min(b->push_distance);
}

void assign(DynamicBoxes& b, int index, bool value) {
    if (value)
        b.set(index);
    else
        b.reset(index);
}

const Level* LoadLevel(string_view filename) {
    LevelEnv env;
    env.Load(filename);
    return LoadLevel(env);
}

const Level* LoadLevel(const LevelEnv& env) {
    Minimal m;
    m.init(env);
    m.move_agent_from_deadend();
    m.remove_deadends();
    m.cleanup_walls();
    int num_dead = m.find_dead_cells();

    // TODO destroy on exception
    Level* level = new Level;
    level->buffer = m.cell;
    level->width = m.w;
    level->cells = m.cells(level);

    level->num_boxes = m.num_boxes();
    if (level->num_boxes == 0) THROW(invalid_argument, "no boxes");
    level->num_goals = m.num_goals();
    if (level->num_boxes != level->num_goals) THROW(invalid_argument, "count(box) != count(goal)");

    if (m.box[m.agent]) THROW(runtime_error, "agent on box2");
    level->num_alive = m.cell_count - num_dead;
    level->start.agent = GetCell(level, m.agent)->id;

    for (Cell* c : level->cells)
        if (c->alive) assign(level->start.boxes, c->id, m.box[c->xy]);

    if (level->start.agent < level->num_alive && level->start.boxes[level->start.agent])
        THROW(runtime_error, "agent(%s) on box", level->start.agent);

    ComputePushDistances(level);
    return level;
}

inline double Choose(uint a, uint b) {
    double s = 1;
    for (uint i = a; i > a - b; i--) s *= i;
    for (uint i = 1; i <= b; i++) s /= i;
    return s;
}

inline double Complexity(const Level* level) {
    return log((level->cells.size() - level->num_boxes) * Choose(level->num_alive, level->num_boxes));
}

void PrintInfo(const Level* level) {
    fmt::print("cells {}, alive {}, boxes {}, choose {}, complexity {}\n", level->cells.size(),
          level->num_alive, level->num_boxes, Choose(level->num_alive, level->num_boxes), round(Complexity(level)));
    Print(level, level->start);
}
