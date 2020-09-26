#include <unordered_map>

#include "core/range.h"
#include "core/small_bfs.h"
#include "core/string.h"

#include "sokoban/level.h"
#include "sokoban/util.h"
#include "sokoban/level_env.h"
#include "sokoban/pair_visitor.h"

using std::string_view;
using std::string;
using std::vector;

namespace Code {
constexpr char Wall = '#';
constexpr char Goal = '.';
constexpr char Space = ' ';
constexpr char Dead = ':';
constexpr char Sink = 'x';
};

// TODO Replace Minimal with LevelEnv
struct Minimal {
    // xy is encoded as x + y * W
    int w, h;
    int agent = 0;
    vector<bool> box;
    vector<char> cell;  // Space, Wall or Goal (and later Dead)
    std::array<int, 4> dirs;
    std::array<int, 8> dirs8;
    int cell_count = 0;
    vector<int2> initial_steps;

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
                if (env.sink(i)) cell[xy] = Code::Sink;
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
    bool sink(int xy) const {
        check(xy);
        return cell[xy] == Code::Sink;
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
            if (m == 1) initial_steps.push_back(int2{1, 0});
            if (m == -1) initial_steps.push_back(int2{-1, 0});
            if (m == -w) initial_steps.push_back(int2{0, -1});
            if (m == w) initial_steps.push_back(int2{0, 1});
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
            if (!goal(i) && !sink(i)) continue;
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

    // note: start can be alive cell!
    vector<Cell*> dead_region(const vector<Cell*>& cells, Cell* start) const {
        small_bfs<Cell*> visitor(cells.size());
        visitor.add(start, start->id);

        vector<Cell*> result;
        for (Cell* a : visitor) {
            if (a->alive && a != start) continue;
            result.push_back(a);
            for (int d = 0; d < 4; d++) {
                if (a->_dir[d]) visitor.add(a->_dir[d], a->_dir[d]->id);
            }
        }
        return result;
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
            c->sink = sink(a);
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

        // init Cell::_dir and Cell::dir8
        for (Cell* c : cells) {
            for (int d = 0; d < 4; d++) c->_dir[d] = open(c->xy + dirs[d]) ? reverse[c->xy + dirs[d]] : nullptr;
            for (int d = 0; d < 8; d++) c->dir8[d] = open(c->xy + dirs8[d]) ? reverse[c->xy + dirs8[d]] : nullptr;
        }

        // init Cell::moves
        for (Cell* c : cells) {
            int m = 0;
            for (int d = 0; d < 4; d++) {
                if (c->dir(d)) m += 1;
            }

            c->moves.resize(m);
            m = 0;
            for (int d = 0; d < 4; d++) {
                if (c->dir(d)) c->moves[m++] = std::pair<int, Cell*>(d, c->dir(d));
            }
        }

        // init Cell::actions
        for (Cell* c : cells) {
            for (Cell* a : dead_region(cells, c)) {
                for (int d = 0; d < 4; d++) {
                    Cell* b = a->_dir[d];
                    if (b && (b->alive || b->sink) && b != c) {
                        c->actions.emplace_back(d, a->_dir[d]);
                    }
                }
            }
        }

        // init Cell::new_moves
        for (Cell* c : cells) {
            for (Cell* a : dead_region(cells, c)) {
                for (int d = 0; d < 4; d++) {
                    Cell* b = a->_dir[d];
                    if (b && (b->alive || b->sink) && a->_dir[d] != c) {
                        c->new_moves.emplace_back(a->_dir[d]);
                    }
                }
            }
            remove_dups(c->new_moves);
        }

        // init Cell::pushes
        for (Cell* c : cells) {
            int p = 0;
            for (int d = 0; d < 4; d++) {
                if (c->dir(d) && (c->dir(d)->alive || c->dir(d)->sink) && c->dir(d ^ 2)) p += 1;
            }
            c->pushes.resize(p);
            p = 0;
            for (int d = 0; d < 4; d++) {
                if (c->dir(d) && (c->dir(d)->alive || c->dir(d)->sink) && c->dir(d ^ 2)) {
                    c->pushes[p++] = std::pair<Cell*, Cell*>(c->dir(d), c->dir(d ^ 2));
                }
            }
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

void ComputePushDistances(Level* level) {
    for (Cell* c : level->cells)
        if (c->alive) c->push_distance.resize(level->num_goals, Cell::Inf);

    matrix<uint> distance;
    distance.resize(level->cells.size(), level->num_alive);
    PairVisitor visitor(level->cells.size(), level->num_alive);

    for (Cell* g : level->goals()) {
        auto goal = g->id;
        visitor.clear();
        distance.fill(Cell::Inf);
        // Uses "moves" as this is reverse search
        for (auto [_, e] : g->moves)
            if (visitor.add(e->id, goal)) distance(e->id, goal) = 0;
        g->push_distance[goal] = 0;

        for (auto [agent, box] : visitor) {
            Cell* a = level->cells[agent];
            minimize(level->cells[box]->push_distance[goal], distance(agent, box));

            // Uses "moves" as this is reverse search
            for (auto [d, n] : a->moves) {
                uint next = n->id;
                if (next != box && visitor.add(next, box))
                    distance(next, box) = distance(agent, box);  // no move cost
                if (a->alive && a->dir(d ^ 2) && a->dir(d ^ 2)->id == box && visitor.add(next, agent))
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

void ComputeGoalPenalties(Level* level) {
    vector<int> goal_max_dist(level->num_goals, 0);
    for (int i = 0; i < level->num_goals; i++) {
        for (Cell* a : level->alive()) {
            if (a->push_distance[i] != Cell::Inf) goal_max_dist[i] = std::max(goal_max_dist[i], int(a->push_distance[i]));
        }
    }

    int next_penalty = 0;
    while (true) {
        int m = -1;
        for (int i = 0; i < level->num_goals; i++) m = std::max(m, goal_max_dist[i]);
        if (m == -1) break;

        for (int i = 0; i < level->num_goals; i++) {
            if (goal_max_dist[i] == m) {
                level->cells[i]->goal_penalty = next_penalty;
                goal_max_dist[i] = -1;
            }
        }
        next_penalty += 1;
    }

    next_penalty -= 1;
    for (int i = level->num_goals; i < level->num_alive; i++) {
        level->cells[i]->goal_penalty = next_penalty;
    }
}

const Level* LoadLevel(string_view filename) {
    LevelEnv env;
    env.Load(filename);
    return LoadLevel(env);
}

const Level* LoadLevel(const LevelEnv& env, bool extra) {
    Minimal m;
    m.init(env);
    if (extra) {
        m.move_agent_from_deadend();
        m.remove_deadends();
        m.cleanup_walls();
    }
    int num_dead = m.find_dead_cells();

    // TODO destroy on exception
    Level* level = new Level;
    level->name = env.name;
    level->buffer = m.cell;
    level->width = m.w;
    level->initial_steps = m.initial_steps;
    level->cells = m.cells(level);

    level->num_boxes = m.num_boxes();
    if (level->num_boxes == 0) THROW(invalid_argument, "no boxes");
    level->num_goals = m.num_goals();

    if (m.box[m.agent]) THROW(runtime_error, "agent on box2");
    level->num_alive = m.cell_count - num_dead;
    level->start.agent = GetCell(level, m.agent)->id;

    for (Cell* c : level->cells)
        if (c->alive) assign(level->start.boxes, c->id, m.box[c->xy]);

    if (level->start.agent < level->num_alive && level->start.boxes[level->start.agent])
        THROW(runtime_error, "agent(%s) on box", level->start.agent);

    if (extra) {
        ComputePushDistances(level);
        ComputeGoalPenalties(level);
    }

    /*string label;
    Print(level, level->start, [&label](auto e) {
        if (e->goal_penalty > 99) label = "??"; else label = format("{}", e->goal_penalty);
        if (label.size() == 1) label = " " + label;
        return label;
    });*/
    return level;
}

void Destroy(const Level* clevel) {
    Level* level = const_cast<Level*>(clevel);
    for (Cell* c : level->cells) delete c;
    delete level;
}

inline double Choose(uint a, uint b) {
    double s = 1;
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
