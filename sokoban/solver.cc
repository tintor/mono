#include "core/array_deque.h"
#include "core/auto.h"
#include "core/bits_util.h"
#include "core/range.h"
#include "core/small_bfs.h"
#include "core/string.h"
#include "core/thread.h"
#include "core/timestamp.h"

#include "sokoban/solver.h"
#include "sokoban/corrals.h"
#include "sokoban/frozen.h"
#include "sokoban/deadlock.h"
#include "sokoban/hungarian.h"
#include "sokoban/util.h"

#include "absl/container/flat_hash_set.h"

using namespace std::chrono_literals;
using std::nullopt;
using std::vector;
using std::array;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::pair;
using std::optional;
using absl::flat_hash_map;
using absl::flat_hash_set;

template <typename State>
struct StateMap {
    constexpr static int SHARDS = 64;
    array<mutex, SHARDS> locks;
    array<flat_hash_map<State, StateInfo>, SHARDS> data;

    static int shard(const State& s) { return fmix64(s.boxes.hash() * 7) % SHARDS; }

    void print_sizes() {
        print("states map");
        for (int i = 0; i < SHARDS; i++) {
            locks[i].lock();
            print(" %h", data[i].size());
            locks[i].unlock();
        }
        print("\n");
    }

    void lock(int shard) {
        Timestamp lock_ts;
        locks[shard].lock();
        overhead += lock_ts.elapsed();
    }

    void lock2(int shard) {
        Timestamp lock_ts;
        locks[shard].lock();
        overhead2 += lock_ts.elapsed();
    }

    void unlock(int shard) { locks[shard].unlock(); }

    bool contains(const State& s, int shard) { return data[shard].find(s) != data[shard].end(); }

    StateInfo get(const State& s, int shard) { return data[shard].at(s); }

    StateInfo* query(const State& s, int shard) {
        auto& d = data[shard];
        auto it = d.find(s);
        if (it == d.end()) return nullptr;
        return &it->second;
    }

    void add(const State& s, const StateInfo& si, int shard) { data[shard].emplace(s, si); }

    long size() {
        long result = 0;
        for (int i = 0; i < SHARDS; i++) {
            locks[i].lock();
            result += data[i].size();
            locks[i].unlock();
        }
        return result;
    }

    void reset() {
        overhead = 0;
        overhead2 = 0;
        for (auto& d : data) d.clear();
    }

    std::atomic<long> overhead = 0;
    std::atomic<long> overhead2 = 0;
};

template <typename T>
void ensure_size(vector<T>& vec, size_t s) {
    if (s > vec.size()) vec.resize(round_up_power2(s));
}

template <typename State>
class StateQueue {
   public:
    StateQueue(uint concurrency) : _concurrency(concurrency) { queue.resize(256); }

    void push(const State& s, uint priority) {
        Timestamp lock_ts;
        queue_lock.lock();
        _overhead += lock_ts.elapsed();

        ensure_size(queue, priority + 1);
        queue[priority].push_back(s);
        if (priority < min_queue) min_queue = priority;
        bool notify = queue_size == 0;
        queue_size += 1;
        queue_lock.unlock();

        if (notify) queue_push_cv.notify_all();
    }

    std::optional<std::pair<State, uint>> top() {
        uint mq;
        auto s = pop(&mq);
        if (s.has_value()) return std::pair<State, uint>{*s, mq};
        return nullopt;
    }

    std::optional<State> pop(uint* top_ptr = nullptr) {
        Timestamp lock_ts;
        std::unique_lock<mutex> lk(queue_lock);
        _overhead2 += lock_ts.elapsed();

        if (queue_size == 0) {
            blocked_on_queue += 1;
            while (queue_size == 0) {
                if (!running) return nullopt;
                if (blocked_on_queue >= _concurrency) {
                    running = false;
                    queue_push_cv.notify_all();
                    return nullopt;
                }
                queue_push_cv.wait(lk);
            }
            blocked_on_queue -= 1;
        }

        if (!running) return nullopt;
        while (queue[min_queue].size() == 0) min_queue += 1;

        if (top_ptr) {
            *top_ptr = min_queue;
            return queue[min_queue][0];
        }
        State s = queue[min_queue].front();
        queue[min_queue].pop_front();
        queue_size -= 1;
        return s;
    }

    size_t size() {
        std::unique_lock<mutex> lk(queue_lock);
        return queue_size;
    }

    void shutdown() {
        std::unique_lock<mutex> lk(queue_lock);
        running = false;
        queue_push_cv.notify_all();
    }

    template <class Rep, class Period>
    bool wait_while_running_for(const std::chrono::duration<Rep, Period>& rel_time) {
        std::unique_lock<mutex> lk(queue_lock);
        if (running) queue_push_cv.wait_for(lk, rel_time);
        return running;
    }

    double overhead() {
        std::unique_lock<mutex> lk(queue_lock);
        return Timestamp::to_s(_overhead);
    }

    double overhead2() {
        std::unique_lock<mutex> lk(queue_lock);
        return Timestamp::to_s(_overhead2);
    }

    void reset() {
        running = true;
        _overhead = 0;
        _overhead2 = 0;
        min_queue = 0;
        blocked_on_queue = 0;
        queue_size = 0;
        queue.clear();
    }

   private:
    bool running = true;
    long _overhead = 0;
    long _overhead2 = 0;
    uint min_queue = 0;
    uint blocked_on_queue = 0;
    const uint _concurrency;
    std::mutex queue_lock;
    uint queue_size = 0;
    vector<mt::array_deque<State>> queue;
    std::condition_variable queue_push_cv;
};

class XAgentVisitor : public each<XAgentVisitor> {
private:
    int pos = 0;
    small_queue<const Exits*> queue;
    std::vector<uchar> visited;  // avoid slower vector<bool> as it is bit compressed!

public:
    XAgentVisitor(uint capacity) : queue(capacity) { visited.resize(capacity, 0); }

    void clear() {
        queue.clear();
        for (auto& e : visited) e = 0;
    }

    bool add(const Cell* a) {
        if (visited[a->dead_region_id]) return false;
        queue.push(&a->exits);
        visited[a->dead_region_id] = 1;
        return true;
    }

    std::optional<std::tuple<const Cell*, const Cell*, int>> next() {
        if (!queue) return std::nullopt;
        if (pos == queue.first()->size()) {
            pos = 0;
            queue.pop();
            if (!queue) return std::nullopt;
        }
        return (*queue.first())[pos++];
    }
};

template <typename T>
struct Protected {
    T _data;
    mutex _mutex;
};

template <typename State>
struct Solver {
    using Boxes = typename State::Boxes;

    const int concurrency;
    const Level* level;
    StateMap<State> states;
    StateQueue<State> queue;
    vector<Counters> counters;
    Boxes goals;
    DeadlockDB<Boxes> deadlock_db;

    Solver(const Level* level, bool single_thread)
            : concurrency(single_thread ? 1 : std::thread::hardware_concurrency())
            , level(level)
            , queue(concurrency)
            , deadlock_db(level) {
        for (Cell* c : level->goals()) goals.set(c->id);
    }

   private:
    void add_more_boxes(Boxes& boxes, const Boxes& goals, int num_boxes, int b0, vector<Boxes>& all_boxes) {
        if (num_boxes == 0) {
            if (!goals.contains(boxes)) all_boxes.push_back(boxes);
            return;
        }

        for (uint b = b0; b < level->num_alive; b++) {
            boxes.set(b);
            if (!is_simple_deadlock(level->cells[b], boxes))
                add_more_boxes(boxes, goals, num_boxes - 1, b + 1, all_boxes);
            boxes.reset(b);
        }
    }

   public:
    // TODO move to utils
    bool contains_deadlock(const State& s, const State& deadlock) {
        return s.boxes.contains(deadlock.boxes) &&
               is_cell_reachable(level->cells[s.agent], level->cells[deadlock.agent], deadlock.boxes);
    }

    // TODO move to utils
    bool contains_deadlock(const State& s, const vector<State>& deadlocks) {
        for (const State& d : deadlocks)
            if (contains_deadlock(s, d)) return true;
        return false;
    }

    void generate_deadlocks(int num_boxes, vector<State>& deadlocks) {
        vector<Boxes> all_boxes;
        Boxes boxes;
        add_more_boxes(boxes, goals, num_boxes, 0, all_boxes);

        vector<pair<State, int>> candidates;
        State s;
        for (const Boxes& my_boxes : all_boxes) {
            AgentVisitor visitor(level);
            int h = heuristic(my_boxes);
            for (uint b = 0; b < level->num_alive; b++)
                if (my_boxes[b])
                    for (auto [_, a] : level->cells[b]->moves)
                        if (!my_boxes[a->id] && !visitor.visited(a)) {
                            // visit everything reachable from A
                            s.boxes = my_boxes;
                            s.agent = a->id;

                            // Normalize s (without clearing visitor)
                            visitor.add(a);
                            for (const Cell* ea : visitor) {
                                if (ea->id < s.agent) s.agent = ea->id;
                                for (auto [_, eb] : ea->moves) {
                                    if (!s.boxes[eb->id]) visitor.add(eb);
                                }
                            }

                            for (auto [_, c] : a->moves)
                                if (!my_boxes[c->id]) {
                                    candidates.emplace_back(s, h);
                                    break;
                                }
                        }
        }

        // order candidates by highest heuristic value first to maximize overlaps in paths
        sort(candidates, [](const pair<State, int>& a, const pair<State, int>& b) { return a.second > b.second; });

        vector<State> new_deadlocks;
        flat_hash_set<State> solvable;
        for (auto [candidate, _] : candidates)
            if (!solvable.contains(candidate) && !contains_deadlock(candidate, deadlocks)) {
                states.reset();
                queue.reset();
                auto result = Solve(candidate, 0, false /*pre_normalize*/);
                if (result.has_value()) {
                    for (const State& p : solution(*result, 2)) solvable.insert(p);
                } else {
                    Print(level, candidate);
                    new_deadlocks.push_back(candidate);
                }
            }
        deadlocks.reserve(deadlocks.size() + new_deadlocks.size());
        std::copy(new_deadlocks.begin(), new_deadlocks.end(), std::back_inserter(deadlocks));
    }

    optional<pair<State, StateInfo>> queue_pop() {
        while (true) {
            optional<State> s = queue.pop();
            if (!s) return nullopt;

            int shard = StateMap<State>::shard(*s);
            states.lock2(shard);
            StateInfo* q = states.query(*s, shard);
            if (!q || q->closed) {
                states.unlock(shard);
                continue;
            }
            StateInfo si = *q;  // copy before unlock
            q->closed = true;
            states.unlock(shard);
            return pair<State, StateInfo>{*s, si};
        }
    }

    void normalize(State& s) const {
        AgentVisitor visitor(level, s.agent);
        for (const Cell* a : visitor) {
            if (a->id < s.agent) s.agent = a->id;
            for (auto [_, b] : a->moves)
                if (!s.boxes[b->id]) visitor.add(b);
        }
    }

    uint heuristic_simple(const Boxes& boxes) {
        uint cost = 0;
        for (uint i = 0; i < level->num_alive; i++)
            if (boxes[i]) cost += level->cells[i]->min_push_distance;
        return cost;
    }

    // excludes frozen goals from costs
    uint heuristic(const Boxes& boxes) {
        vector<uint> goal;  // TODO avoid this alloc
        for (Cell* g : level->goals())
            if (!boxes[g->id] || !is_frozen_on_goal_simple(g, boxes)) goal.push_back(g->id);
        if (goal.size() == level->num_goals) return heuristic_simple(boxes);

        uint cost = 0;
        for (Cell* box : level->alive())
            if (boxes[box->id] && !box->goal) {
                // min push distance out of all non-frozen goals
                uint dist = Cell::Inf;
                for (uint i = 0; i < goal.size(); i++) minimize(dist, box->push_distance[goal[i]]);
                cost += dist;
            }
        return cost;
    }

    void PrintWithCorral(const State& s, const optional<Corral>& corral) {
        Print(level, s, [&](Cell* c) {
            if (!corral.has_value() || !(*corral)[c->id]) return "";
            if (s.boxes[c->id]) return c->goal ? "🔷" : "⚪";
            if (c->goal) return "❔";
            if (!c->alive) return "❕";
            return "▫️ ";
        });
    }

    void Monitor(int verbosity, const Timestamp& start_ts) {
        Corrals<State> corrals(level);
        bool running = verbosity > 0;
        if (!running) return;

        while (running) {
            if (!queue.wait_while_running_for(5s)) running = false;
            long seconds = std::lround(start_ts.elapsed_s());
            if (seconds < 4 && running) continue;

            auto total = states.size();
            auto open = queue.size();
            auto closed = (total >= open) ? total - open : 0;

            print("{}: states {} ({} {} {:.1f})\n", level->name, total, closed, open, 100. * open / total);

            Counters q;
            for (const Counters& c : counters) q.add(c);
            ulong else_ticks = q.total_ticks;
            else_ticks -= q.queue_pop_ticks;
            else_ticks -= q.corral_ticks;
            else_ticks -= q.corral2_ticks;
            else_ticks -= q.is_simple_deadlock_ticks;
            else_ticks -= q.is_reversible_push_ticks;
            else_ticks -= q.contains_frozen_boxes_ticks;
            else_ticks -= q.norm_ticks;
            else_ticks -= q.states_query_ticks;
            else_ticks -= q.heuristic_ticks;
            else_ticks -= q.state_insert_ticks;
            else_ticks -= q.queue_push_ticks;
            q.else_ticks = else_ticks;

            print("elapsed {} ", seconds);
            q.print();
            print("db_size {}, db_summary {}", deadlock_db.size(), deadlock_db.summary());
            print(", locks ({} {}", states.overhead, states.overhead2);
            print(" {:.3f} {:.3f})\n", queue.overhead(), queue.overhead2());
            // states.print_sizes();
            if (verbosity >= 2) {
                auto ss = queue.top();
                if (ss.has_value()) {
                    State s = ss->first;

                    int shard = StateMap<State>::shard(s);
                    states.lock(shard);
                    StateInfo* q = states.query(s, shard);
                    states.unlock(shard);
                    print("queue cost {}, distance {}, heuristic {}\n", ss->second, q->distance, q->heuristic);
                    corrals.find_unsolved_picorral(s);
                    PrintWithCorral(s, corrals.opt_picorral());
                }
            }
        }
    }

    struct WorkerState {
        Corrals<State> corrals;
        Counters* counters = nullptr;
        Protected<optional<pair<State, StateInfo>>>* result = nullptr;

        WorkerState(const Level* level) : corrals(level) {}
    };

    // Returns false is push is a deadlock.
    bool EvaluatePush(const State& s, const StateInfo& si, const Cell* a, const Cell* b, const int d, WorkerState& ws) {
        Counters& q = *ws.counters;
        const Cell* c = b->dir(d);
        if (!c || !c->alive || s.boxes[c->id]) return true;
        if (ws.corrals.has_picorral() && !ws.corrals.picorral()[c->id]) {
            q.corral_cuts += 1;
            return true;
        }
        State ns(b->id, s.boxes);
        ns.boxes.reset(b->id);
        ns.boxes.set(c->id);

        if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, d, q)) return false;

        Timestamp norm_ts;
        normalize(ns);

        Timestamp states_query_ts;
        q.norm_ticks += norm_ts.elapsed(states_query_ts);

        int shard = StateMap<State>::shard(ns);
        states.lock(shard);

        constexpr int Overestimate = 2;
        StateInfo* qs = states.query(ns, shard);
        if (qs) {
            q.duplicates += 1;
            if (si.distance + 1 >= qs->distance) {
                // existing state
                states.unlock(shard);
            } else {
                qs->dir = d;
                qs->distance = si.distance + 1;
                // no need to update heuristic
                qs->prev_agent = a->id;
                states.unlock(shard);
                queue.push(ns, uint(qs->distance) + uint(qs->heuristic) * Overestimate);
                q.updates += 1;
            }
            q.states_query_ticks += states_query_ts.elapsed();
            return true;
        }

        // new state
        StateInfo nsi;
        nsi.dir = d;
        nsi.distance = si.distance + 1;

        Timestamp heuristic_ts;
        q.states_query_ticks += states_query_ts.elapsed(heuristic_ts);

        // TODO holding lock while computing heuristic!
        uint h = heuristic(ns.boxes);
        q.heuristic_ticks += heuristic_ts.elapsed();

        if (h == Cell::Inf) {
            states.unlock(shard);
            q.heuristic_deadlocks += 1;
            // TODO store pattern in deadlock_db
            return false;
        }
        nsi.heuristic = h;

        nsi.prev_agent = a->id;

        Timestamp state_insert_ts;
        states.add(ns, nsi, shard);
        states.unlock(shard);

        Timestamp queue_push_ts;
        q.state_insert_ticks += state_insert_ts.elapsed(queue_push_ts);

        queue.push(ns, int(nsi.distance) + int(nsi.heuristic) * Overestimate);
        q.queue_push_ticks += queue_push_ts.elapsed();

        if (goals.contains(ns.boxes)) {
            queue.shutdown();
            auto& result = *ws.result;
            std::unique_lock<mutex> lock(result._mutex);
            if (!result._data.has_value()) result._data = pair<State, StateInfo>{ns, nsi};
        }
        return true;
    }

    optional<pair<State, StateInfo>> Solve(State start, int verbosity = 0, bool pre_normalize = true) {
        if (concurrency == 1) print(warning, "Warning: Single-threaded!\n");
        Timestamp start_ts;
        if (pre_normalize) normalize(start);
        states.add(start, StateInfo(), StateMap<State>::shard(start));
        queue.push(start, 0);

        if (start.boxes == goals) return pair<State, StateInfo>{start, StateInfo()};

        Protected<optional<pair<State, StateInfo>>> result;

        counters.resize(std::thread::hardware_concurrency());
        std::thread monitor([verbosity, this, start_ts]() { Monitor(verbosity, start_ts); });

        parallel(concurrency, [&](size_t thread_id) {
            Counters& q = counters[thread_id];
            AgentVisitor agent_visitor(level);
            WorkerState ws(level);
            ws.result = &result;
            ws.counters = &q;

            while (true) {
                Timestamp queue_pop_ts;
                ON_SCOPE_EXIT(q.total_ticks += queue_pop_ts.elapsed());

                auto p = queue_pop();
                if (!p) return;
                const State& s = p->first;
                const StateInfo& si = p->second;

                // TODO heuristic: order goals somehow (corner goals first) and try to find solution in goal order
                // -> if that doesn't work then do a regular search

                // corral = unreachable area surrounded by boxes which are either:
                // - frozen on goal OR (reachable by agent and only pushable inside)

                // if corral contains a goal (assuming num_boxes == num_goals) or one of its fence boxes isn't on goal
                // then that corral must be prioritized for push (ignoring all other corrals)

                Timestamp corral_ts;
                q.queue_pop_ticks += queue_pop_ts.elapsed(corral_ts);
                ws.corrals.find_unsolved_picorral(s);
                q.corral_ticks += corral_ts.elapsed();

                agent_visitor.clear();
                bool deadlock = true;
                agent_visitor.add(s.agent);
                for (const Cell* a : agent_visitor) {
                    for (auto [d, b] : a->moves) {
                        if (!s.boxes[b->id]) {
                            agent_visitor.add(b);
                            continue;
                        }
                        if (EvaluatePush(s, si, a, b, d, ws)) deadlock = false;
                    }
                }
                if (deadlock) {
                    // Add (potentially non-minimal) deadlock pattern
                    // TODO: remove any reversible boxes (that can be pushed once and then pushed back to original position)
                    // TODO: if any reversible box is pushed, verify that resulting state is still a deadlock
                    //deadlock_db.AddPattern(s.agent, s.boxes);
                }
            }
        });
        monitor.join();
        return result._data;
    }

    pair<State, StateInfo> previous(pair<State, StateInfo> p) {
        auto [s, si] = p;
        if (si.distance <= 0) THROW(runtime_error, "non-positive distance");
        normalize(s);

        State ps;
        ps.agent = si.prev_agent;
        ps.boxes = s.boxes;
        const Cell* a = level->cells[ps.agent];
        if (!a) THROW(runtime_error, "A null");
        const Cell* b = a->dir(si.dir);
        if (!b) THROW(runtime_error, "B null");
        if (ps.boxes[b->id]) THROW(runtime_error, "box on B");
        const Cell* c = b->dir(si.dir);
        if (!c) THROW(runtime_error, "C null");
        if (!ps.boxes[c->id]) THROW(runtime_error, "no box on C");
        ps.boxes.reset(c->id);
        ps.boxes.set(b->id);
        State norm_ps = ps;
        normalize(norm_ps);
        return {ps, states.get(norm_ps, StateMap<State>::shard(norm_ps))};
    }

    Solution solution(pair<State, StateInfo> s, int verbosity) {
        Timestamp ts;
        vector<DynamicState> result;
        while (true) {
            result.push_back(s.first);
            if (s.second.distance == 0) break;
            s = previous(s);
        }
        std::reverse(result.begin(), result.end());

        if (result.size() >= 2) {
            DynamicState& v = result[result.size() - 2];
            DynamicState& w = result[result.size() - 1];
            for (int i = 0; i < level->alive().size(); i++) {
                if (v.boxes[i] && !w.boxes[i]) {
                    w.agent = i;
                    break;
                }
            }
        }
        Timestamp te;
        return result;
    }
};

template <typename Boxes>
Solution InternalSolve(const Level* level, int verbosity, bool single_thread) {
    if (verbosity > 0) PrintInfo(level);
    Solver<TState<Boxes>> solver(level, single_thread);
    auto solution = solver.Solve(level->start, verbosity);
    if (solution) return solver.solution(*solution, verbosity);
    return {};
}

Solution Solve(const Level* level, int verbosity, bool single_thread) {
#define DENSE(N) \
    if (level->num_alive <= 32 * N) { print("Using DenseBoxes<{}>\n", 32 * N); return InternalSolve<DenseBoxes<32 * N>>(level, verbosity, single_thread); }

    DENSE(1);
    DENSE(2);
    DENSE(3);
    DENSE(4);
    DENSE(5);
    DENSE(6);
    DENSE(7);
    DENSE(8);
#undef DENSE

    print(warning, "Warning: Using DynamicBoxes\n");
    return InternalSolve<DynamicBoxes>(level, verbosity, single_thread);
}

#if 0
Solution Solve(const Level* level, int verbosity, bool single_thread) {
    const int dense_size = (level->num_alive + 31) / 32;

    double f = (level->num_alive <= 256) ? 1 : 2;
    const int sparse_size = ceil(ceil(level->num_boxes * f) / 4);

    if (dense_size <= sparse_size && dense_size <= 32) {
#define DENSE(N) \
    if (level->num_alive <= 32 * N) { print("Using DenseBoxes<{}>\n", 32 * N); return InternalSolve<DenseBoxes<32 * N>>(level, verbosity, single_thread); }
        DENSE(1);
        DENSE(2);
        DENSE(3);
        DENSE(4);
        DENSE(5);
        DENSE(6);
        DENSE(7);
        DENSE(8);
        THROW(not_implemented);
#undef DENSE
    }

    if (level->num_alive < 256) {
#define SPARSE(N) \
    if (level->num_boxes <= 4 * N) { print("Using SparseBoxes<uchar, {}>\n", 4 * N); return InternalSolve<SparseBoxes<uchar, 4 * N>>(level, verbosity, single_thread); }
        SPARSE(1);
        SPARSE(2);
        SPARSE(3);
        THROW(not_implemented);
#undef SPARSE
    }

    if (level->num_alive < 256 * 256) {
#define SPARSE(N) \
    if (level->num_boxes <= 2 * N) { print("Using SparseBoxes<ushort, {}>\n", 2 * N); return InternalSolve<SparseBoxes<ushort, 2 * N>>(level, verbosity, single_thread); }
        SPARSE(1);
        SPARSE(2);
        SPARSE(3);
        SPARSE(4);
        SPARSE(5);
        SPARSE(6);
        SPARSE(7);
        SPARSE(8);
        THROW(not_implemented);
#undef SPARSE
    }

    print(warning, "Warning: Using DynamicBoxes\n");
    return InternalSolve<DynamicBoxes>(level, verbosity, single_thread);
}
#endif

template<typename Boxes>
vector<const Cell*> ShortestPath(const Level* level, const Cell* start, const Cell* end, const Boxes& boxes) {
    if (boxes[start->id] || boxes[end->id]) THROW(runtime_error, "precondition");

    vector<const Cell*> prev(level->cells.size());
    AgentVisitor visitor(start);
    for (const Cell* a : visitor) {
        for (auto [d, b] : a->moves) {
            if (!boxes[b->id] && visitor.add(b)) prev[b->id] = a;
            if (b != end) continue;
            vector<const Cell*> path;
            for (const Cell* p = end; p != start; p = prev[p->id]) path.push_back(p);
            std::reverse(path.begin(), path.end());
            return path;
        }
    }
    return {};
}

void ExtractMoves(const Level* level, LevelEnv& env, const DynamicState& dest, vector<int2>& steps) {
    const Cell* agent = level->cell_by_xy(env.agent);
    for (const Cell* step : ShortestPath(level, agent, level->cells[dest.agent], dest.boxes)) {
        int2 delta = level->cell_to_vec(step) - env.agent;
        if (!env.Move(level->cell_to_vec(step) - env.agent)) THROW(runtime_error, "move failed");
        steps.push_back(delta);
    }
}

void ExtractPush(const Level* level, LevelEnv& env, const DynamicState& state0, const DynamicState& state1, vector<int2>& steps) {
    const Cell* src = nullptr;
    const Cell* dest = nullptr;
    for (int j = 0; j < level->alive().size(); j++) {
        if (state0.boxes[j] && !state1.boxes[j]) src = level->cells[j];
        if (!state0.boxes[j] && state1.boxes[j]) dest = level->cells[j];
    }
    if (!src || !dest || src == dest) THROW(runtime_error, "oops!");

    int2 delta = level->cell_to_vec(dest) - level->cell_to_vec(src);
    if (!env.Push(delta)) THROW(runtime_error, "push failed (delta {} {})", int(delta.x), int(delta.y));
    steps.push_back(delta);
}

// Returns steps as deltas and number of pushes.
pair<vector<int2>, int> Solve(LevelEnv env, int verbosity, bool single_thread) {
    auto level = LoadLevel(env);
    auto pushes = Solve(level, verbosity, single_thread);
    if (pushes.empty()) return {};

    Timestamp ts;
    vector<int2> steps;
    for (int2 step : level->initial_steps) {
        if (!env.Action(step)) THROW(runtime_error, "initial step failed");
        steps.push_back(step);
    }
    ExtractMoves(level, env, pushes[0], steps);
    for (int i = 1; i < pushes.size(); i++) {
        ExtractPush(level, env, pushes[i - 1], pushes[i], steps);
        ExtractMoves(level, env, pushes[i], steps);
    }
    if (!env.IsSolved()) THROW(runtime_error, "not solved!");
    Timestamp te;
    return {std::move(steps), pushes.size()};
}

template <typename Cell>
void InternalGenerateDeadlocks(const Level* level, bool single_thread) {
    const int MaxBoxes = 4;
    Timestamp ts;
    using State = TState<DynamicBoxes>;
    // using State = TState<SparseBoxes<Cell, MaxBoxes>>;
    Solver<State> solver(level, single_thread);
    vector<State> deadlocks;
    for (auto boxes : range(1, MaxBoxes + 1)) solver.generate_deadlocks(boxes, deadlocks);
    print("found deadlocks {} in {}\n", deadlocks.size(), ts.elapsed());
}

void GenerateDeadlocks(const Level* level, bool single_thread) {
    if (level->num_alive < 256)
        InternalGenerateDeadlocks<uchar>(level, single_thread);
    else
        InternalGenerateDeadlocks<ushort>(level, single_thread);
}
