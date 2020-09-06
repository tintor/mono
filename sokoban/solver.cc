#include "fmt/core.h"

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
#include "sokoban/hungarian.h"
#include "sokoban/util.h"

#include "absl/container/flat_hash_set.h"

#include <shared_mutex>

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
using fmt::print;

class DeadlockDB {
    using Word = uint;
    constexpr static int WordBits = sizeof(Word) * 8;

public:
    DeadlockDB(const Level* level)
        : _level(level)
        , _num_alive(level->alive().size())
        , _agent_words((_level->cells.size() + WordBits - 1) / WordBits)
        , _box_words((_num_alive + WordBits - 1) / WordBits) {
        _words.reserve((_agent_words + _box_words) * 64);
    }

    template <typename State>
    bool ContainsPattern(const State& s) {
        array<Word, 100> boxes_static;
        vector<Word> boxes_dynamic;
        Word* boxes;
        if (_num_alive <= boxes_static.size()) {
            boxes = boxes_static.data();
        } else {
            boxes_dynamic.resize(_num_alive, 0);
            boxes = boxes_dynamic.data();
        }

        std::fill(boxes, boxes + _box_words, 0);
        for (int i = 0; i < _num_alive; i++) {
            if (s.boxes[i]) AddBit(boxes, i);
        }

        _mutex.lock_shared();
        const Word* end = _words.data() + _words.size();
        for (const Word* p = _words.data(); p < end; p += _agent_words + _box_words) {
            if (!MatchesPattern(s.agent, boxes, p)) continue;
            _mutex.unlock_shared();
            return true;
        }
        _mutex.unlock_shared();
        return false;
    }

    // Note: Pattern must be minimal and not already in database.
    template <typename State>
    void AddPattern(const State& s) {
        if (ContainsPattern(s)) THROW(runtime_error, "duplicate deadlock pattern");
        _mutex.lock();
        _words.resize(_words.size() + _agent_words + _box_words);
        WritePattern(s, _words.data() + _words.size() - _agent_words - _box_words);
        _mutex.unlock();
        if (!ContainsPattern(s)) THROW(runtime_error, "pattern not stored");
    }

private:
    bool MatchesPattern(const int agent, const Word* boxes, const Word* p) {
        if (!HasBit(p, agent)) return false;

        const Word* pb = p + _agent_words;
        for (int i = 0; i < _box_words; i++) {
            if ((boxes[i] | pb[i]) != boxes[i]) return false;
        }
        return true;
    }

    template <typename State>
    void WritePattern(const State& s, Word* p) {
        std::fill(p, p + _agent_words + _box_words, 0);

        // Agent part
        small_bfs<const Cell*> visitor(_level->cells.size());
        visitor.add(_level->cells[s.agent], s.agent);
        for (const Cell* a : visitor) {
            AddBit(p, a->id);
            for (auto [_, b] : a->moves) {
                if (!s.boxes[b->id]) visitor.add(b, b->id);
            }
        }

        // Box part
        for (int i = 0; i < _num_alive; i++) {
            if (s.boxes[i]) AddBit(p + _agent_words, i);
        }

        print("New deadlock pattern:\n");
        Print(_level, s, [&](const Cell* e) {
            if (HasBit(p, e->id)) return "▫️ ";
            if (e->id < _num_alive && HasBit(p + _agent_words, e->id)) return "🔵";
            return "  ";
        });
    }

    static bool HasBit(const Word* p, int index) {
        return p[index / WordBits] & (Word(1) << (index & WordBits));
    }

    static void AddBit(Word* p, int index) {
        p[index / WordBits] |= Word(1) << (index % WordBits);
    }

    const Level* _level;
    const int _num_alive;
    const int _agent_words;
    const int _box_words;

    std::shared_mutex _mutex;
    vector<Word> _words;
};

double Sec(ulong ticks) { return Timestamp::to_s(ticks); }

template <typename State>
struct StateMap {
    constexpr static int SHARDS = 64;
    array<mutex, SHARDS> locks;
    array<flat_hash_map<State, StateInfo>, SHARDS> data;

    static int shard(const State& s) { return fmix64(s.boxes.hash() * 7) % SHARDS; }

    void print_sizes() {
        fmt::print("states map");
        for (int i = 0; i < SHARDS; i++) {
            locks[i].lock();
            fmt::print(" %h", data[i].size());
            locks[i].unlock();
        }
        fmt::print("\n");
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
    uint queue_size = 0;
    vector<mt::array_deque<State>> queue;
    std::mutex queue_lock;
    std::condition_variable queue_push_cv;
};

struct Counters {
    typedef ulong T;

    T simple_deadlocks = 0;
    T frozen_box_deadlocks = 0;
    T heuristic_deadlocks = 0;
    T corral_cuts = 0;
    T duplicates = 0;
    T updates = 0;

    T total_ticks = 0;
    T queue_pop_ticks = 0;
    T corral_ticks = 0;
    T corral2_ticks = 0;
    T is_simple_deadlock_ticks = 0;
    T is_reversible_push_ticks = 0;
    T contains_frozen_boxes_ticks = 0;
    T norm_ticks = 0;
    T states_query_ticks = 0;
    T heuristic_ticks = 0;
    T state_insert_ticks = 0;
    T queue_push_ticks = 0;

    Counters() { memset(this, 0, sizeof(Counters)); }

    void add(const Counters& src) {
        const T* s = reinterpret_cast<const T*>(&src);
        const T* e = s + sizeof(Counters) / sizeof(T);
        static_assert(sizeof(Counters) == sizeof(T) * 18);
        T* d = reinterpret_cast<T*>(this);
        while (s != e) *d++ += *s++;
    }
};

const static uint kConcurrency = std::thread::hardware_concurrency();

class AgentVisitor : public each<AgentVisitor> {
private:
    int pos = 0;
    small_queue<const Exits*> queue;
    std::vector<uchar> visited;  // avoid slower vector<bool> as it is bit compressed!

public:
    AgentVisitor(uint capacity) : queue(capacity) { visited.resize(capacity, 0); }

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

template <typename State>
struct Solver {
    using Boxes = typename State::Boxes;

    const Level* level;
    StateMap<State> states;
    StateQueue<State> queue;
    vector<Counters> counters;
    small_bfs<const Cell*> visitor;
    Boxes goals;
    DeadlockDB deadlock_db;

    Solver(const Level* level) : level(level), queue(kConcurrency), visitor(level->cells.size()), deadlock_db(level) {
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
    // Note: uses visitor
    bool contains_deadlock(const State& s, const State& deadlock) {
        return s.boxes.contains(deadlock.boxes) &&
               is_cell_reachable(level->cells[s.agent], level->cells[deadlock.agent], deadlock.boxes, visitor);
    }

    // TODO move to utils
    // Note: uses visitor
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
            visitor.clear();
            int h = heuristic(my_boxes);
            for (uint b = 0; b < level->num_alive; b++)
                if (my_boxes[b])
                    for (auto [_, a] : level->cells[b]->moves)
                        if (!my_boxes[a->id] && !visitor.visited[a->id]) {
                            // visit everything reachable from A
                            s.boxes = my_boxes;
                            s.agent = a->id;
                            normalize(s, visitor, false /*don't clear*/);
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
                auto result = solve(candidate, 0, false /*pre_normalize*/);
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

    void normalize(State& s, small_bfs<const Cell*>& visitor, bool clear = true) {
        if (clear) visitor.clear();
        visitor.add(level->cells[s.agent], s.agent);
        for (const Cell* a : visitor) {
            if (a->id < s.agent) s.agent = a->id;
            for (auto [_, b] : a->moves)
                if (!s.boxes[b->id]) visitor.add(b, b->id);
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

    void Monitor(int verbosity, mutex& result_lock, const Timestamp& start_ts) {
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

            lock_guard g(result_lock);
            print("states {} ({} {} {:.1f})\n", total, closed, open, 100. * open / total);

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

            print(
                "elapsed {} ({:.1f} | queue_pop {:.1f}, corral {:.1f} {:.1f}, "
                "is_simple_deadlock {:.1f}, is_reversible_push {:.1f}, contains_frozen_boxes {:.1f}, "
                "norm {:.1f}, states_query {:.1f}, heuristic {:.1f}, "
                "state_insert {:.1f}, queue_push {:.1f}, else {:.1f})\n",
                seconds, Sec(q.total_ticks), Sec(q.queue_pop_ticks), Sec(q.corral_ticks), Sec(q.corral2_ticks),
                Sec(q.is_simple_deadlock_ticks), Sec(q.is_reversible_push_ticks), Sec(q.contains_frozen_boxes_ticks), Sec(q.norm_ticks),
                Sec(q.states_query_ticks), Sec(q.heuristic_ticks), Sec(q.state_insert_ticks), Sec(q.queue_push_ticks), Sec(else_ticks));
            print("deadlocks (simple {}, frozen_box {}, heuristic {})", q.simple_deadlocks, q.frozen_box_deadlocks,
                  q.heuristic_deadlocks);
            print(", corral cuts {}, dups {}, updates {}", q.corral_cuts, q.duplicates, q.updates);
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

    optional<pair<State, StateInfo>> solve(State start, int verbosity = 0, bool pre_normalize = true) {
        if (kConcurrency == 1) print("Warning: Single-threaded!\n");
        Timestamp start_ts;
        if (pre_normalize) normalize(start, visitor);
        states.add(start, StateInfo(), StateMap<State>::shard(start));
        queue.push(start, 0);

        if (start.boxes == goals) return pair<State, StateInfo>{start, StateInfo()};

        optional<pair<State, StateInfo>> result;
        mutex result_lock;

        counters.resize(std::thread::hardware_concurrency());
        std::thread monitor([verbosity, this, &result_lock, start_ts]() { Monitor(verbosity, result_lock, start_ts); });

        parallel(kConcurrency, [&](size_t thread_id) {
            Counters& q = counters[thread_id];
//#define AGENT_VISITOR
#ifdef AGENT_VISITOR
            AgentVisitor agent_visitor(level->cells.size());
#else
            small_bfs<const Cell*> agent_visitor(level->cells.size());
#endif
            small_bfs<const Cell*> tmp_visitor(level->cells.size());
            Corrals<State> corrals(level);

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
                corrals.find_unsolved_picorral(s);
                q.corral_ticks += corral_ts.elapsed();

                agent_visitor.clear();
#ifdef AGENT_VISITOR
                agent_visitor.add(level->cells[s.agent]/*, s.agent*/);
                for (auto [a, d, b] : agent_visitor) {
                        if (!s.boxes[b->id]) {
                            agent_visitor.add(b/*, b->id*/);
#else
                agent_visitor.add(level->cells[s.agent], s.agent);
                for (const Cell* a : agent_visitor) {
                    for (auto [d, b] : a->moves) {
                        if (!s.boxes[b->id]) {
                            agent_visitor.add(b, b->id);
#endif
                            continue;
                        }
                        // push
                        const Cell* c = b->dir(d);
                        if (!c || !c->alive || s.boxes[c->id]) continue;
                        if (corrals.has_picorral() && !corrals.picorral()[c->id]) {
                            q.corral_cuts += 1;
                            continue;
                        }
                        State ns(b->id, s.boxes);
                        ns.boxes.reset(b->id);
                        ns.boxes.set(c->id);

                        Timestamp deadlock_ts;
                        if (TIMER(is_simple_deadlock(c, ns.boxes), q.is_simple_deadlock_ticks)) {
                            q.simple_deadlocks += 1;
                            continue;
                        }
                        auto dd = d;
                        auto bb = b;
                        if (TIMER(!is_reversible_push(level->cells[ns.agent], ns.boxes, dd, tmp_visitor),
                                  q.is_reversible_push_ticks) &&
                            TIMER(contains_frozen_boxes(bb, ns.boxes, level->num_goals, level->num_alive, tmp_visitor),
                                  q.contains_frozen_boxes_ticks)) {
                            q.frozen_box_deadlocks += 1;
                            continue;
                        }

                        Timestamp norm_ts;
                        normalize(ns, tmp_visitor);

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
                            continue;
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
                            continue;
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
                            lock_guard g(result_lock);
                            if (!result) result = pair<State, StateInfo>{ns, nsi};
                            return;
                        }
#ifndef AGENT_VISITOR
                    }
#endif
                }
            }
        });
        monitor.join();
        return result;
    }

    pair<State, StateInfo> previous(pair<State, StateInfo> p) {
        auto [s, si] = p;
        if (si.distance <= 0) THROW(runtime_error, "non-positive distance");
        normalize(s, visitor);

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
        normalize(norm_ps, visitor);
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
Solution InternalSolve(const Level* level, int verbosity) {
    if (verbosity > 0) PrintInfo(level);
    Solver<TState<Boxes>> solver(level);
    auto solution = solver.solve(level->start, verbosity);
    if (solution) return solver.solution(*solution, verbosity);
    return {};
}

Solution Solve(const Level* level, int verbosity) {
#ifdef OPTIMIZE_MEMORY
    const int dense_size = (level->num_alive + 31) / 32;

    double f = (level->num_alive <= 256) ? 1 : 2;
    const int sparse_size = ceil(ceil(level->num_boxes * f) / 4);

    if (dense_size <= sparse_size && dense_size <= 32) {
#define DENSE(N) \
    if (level->num_alive <= 32 * N) return InternalSolve<DenseBoxes<32 * N>>(level, verbosity)
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
    if (level->num_boxes <= 4 * N) return InternalSolve<SparseBoxes<uchar, 4 * N>>(level, verbosity)
        SPARSE(1);
        SPARSE(2);
        SPARSE(3);
        THROW(not_implemented);
#undef SPARSE
    }

    if (level->num_alive < 256 * 256) {
#define SPARSE(N) \
    if (level->num_boxes <= 2 * N) return InternalSolve<SparseBoxes<ushort, 2 * N>>(level, verbosity)
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

    THROW(not_implemented);
#else
    return InternalSolve<DynamicBoxes>(level, verbosity);
#endif
}

template<typename Boxes>
vector<const Cell*> ShortestPath(const Level* level, const Cell* start, const Cell* end, const Boxes& boxes) {
    if (boxes[start->id] || boxes[end->id]) THROW(runtime_error, "precondition");

    vector<const Cell*> prev(level->cells.size());
    small_bfs<const Cell*> visitor(level->cells.size());
    visitor.add(start, start->id);
    for (const Cell* a : visitor) {
        for (auto [d, b] : a->moves) {
            if (!boxes[b->id] && visitor.add(b, b->id)) prev[b->id] = a;
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
pair<vector<int2>, int> Solve(LevelEnv env, int verbosity) {
    auto level = LoadLevel(env);
    auto pushes = Solve(level, verbosity);
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
void InternalGenerateDeadlocks(const Level* level) {
    const int MaxBoxes = 4;
    Timestamp ts;
    using State = TState<DynamicBoxes>;
    // using State = TState<SparseBoxes<Cell, MaxBoxes>>;
    Solver<State> solver(level);
    vector<State> deadlocks;
    for (auto boxes : range(1, MaxBoxes + 1)) solver.generate_deadlocks(boxes, deadlocks);
    print("found deadlocks {} in {}\n", deadlocks.size(), ts.elapsed());
}

void GenerateDeadlocks(const Level* level) {
    if (level->num_alive < 256)
        InternalGenerateDeadlocks<uchar>(level);
    else
        InternalGenerateDeadlocks<ushort>(level);
}
