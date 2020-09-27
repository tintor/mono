#include "core/auto.h"
#include "core/bits_util.h"
#include "core/range.h"

#include "sokoban/common.h"
#include "sokoban/solver.h"
#include "sokoban/corrals.h"
#include "sokoban/frozen.h"
#include "sokoban/deadlock.h"
#include "sokoban/hungarian.h"
#include "sokoban/util.h"
#include "sokoban/heuristic.h"
#include "sokoban/state_map.h"
#include "sokoban/level_loader.h"
#include "sokoban/level_printer.h"

#include "ctpl.h"

template <typename T>
void ensure_size(vector<T>& vec, size_t s) {
    if (s > vec.size()) vec.resize(round_up_power2(s));
}

template <typename State>
class ConcurrentStateQueue {
   public:
    ConcurrentStateQueue(uint concurrency) : _concurrency(concurrency) { queue.resize(256); }

    void push(const State& s, uint priority) {
        Timestamp lock_ts;
        queue_lock.lock();
        _push_overhead += lock_ts.elapsed();

        ensure_size(queue, priority + 1);
        queue[priority].push_back(s);
        if (priority < min_queue) min_queue = priority;
        bool notify = queue_size == 0;
        queue_size += 1;
        queue_lock.unlock();

        if (notify) queue_push_cv.notify_all();
    }

    optional<State> top() const {
        Timestamp lock_ts;
        unique_lock<mutex> lk(queue_lock);
        _pop_overhead += lock_ts.elapsed();
        if (block_if_empty(lk)) return nullopt;
        return queue[min_queue][0];
    }

    optional<State> pop() {
        Timestamp lock_ts;
        unique_lock<mutex> lk(queue_lock);
        _pop_overhead += lock_ts.elapsed();
        if (block_if_empty(lk)) return nullopt;

        State s = queue[min_queue].front();
        queue[min_queue].pop_front();
        queue_size -= 1;
        return s;
    }

    size_t size() const {
        unique_lock<mutex> lk(queue_lock);
        return queue_size;
    }

    void shutdown() {
        unique_lock<mutex> lk(queue_lock);
        running = false;
        queue_push_cv.notify_all();
    }

    template <class Rep, class Period>
    bool wait_while_running_for(const std::chrono::duration<Rep, Period>& rel_time) const {
        unique_lock<mutex> lk(queue_lock);
        if (running) queue_push_cv.wait_for(lk, rel_time);
        return running;
    }

    void reset() {
        running = true;
        _push_overhead = 0;
        _pop_overhead = 0;
        min_queue = 0;
        blocked_on_queue = 0;
        queue_size = 0;
        queue.clear();
    }

    string monitor() const {
        unique_lock<mutex> lk(queue_lock);
        return format("push {:.3f}, pop {:.3f}", Timestamp::to_s(_push_overhead), Timestamp::to_s(_pop_overhead));
    }

   private:
    bool block_if_empty(unique_lock<mutex>& lk) const {
        if (queue_size == 0) {
            blocked_on_queue += 1;
            while (queue_size == 0) {
                if (!running) return true;
                if (blocked_on_queue >= _concurrency) {
                    running = false;
                    queue_push_cv.notify_all();
                    return true;
                }
                queue_push_cv.wait(lk);
            }
            blocked_on_queue -= 1;
        }
        if (!running) return true;
        while (queue[min_queue].size() == 0) min_queue += 1;
        return false;
    }

    const uint _concurrency;

    mutable bool running = true;
    mutable long _push_overhead = 0;
    mutable long _pop_overhead = 0;
    mutable uint blocked_on_queue = 0;
    mutable mutex queue_lock;
    mutable condition_variable queue_push_cv;

    mutable uint min_queue = 0;
    uint queue_size = 0;
    vector<array_deque<State>> queue;
};

template <typename State>
pair<State, StateInfo> Previous(pair<State, StateInfo> p, const Level* level, const StateMap<State>& states) {
    auto [s, si] = p;
    if (si.distance <= 0) THROW(runtime_error, "non-positive distance");
    normalize(level, &s.agent, s.boxes);

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
    normalize(level, &norm_ps.agent, norm_ps.boxes);
    return {ps, states.get(norm_ps, StateMap<State>::shard(norm_ps))};
}

template <typename State>
Solution ExtractSolution(pair<State, StateInfo> s, const Level* level, const StateMap<State>& states) {
    Timestamp ts;
    vector<DynamicState> result;
    while (true) {
        result.push_back(s.first);
        if (s.second.distance == 0) break;
        s = Previous(s, level, states);
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
    return result;
}

template <typename State, typename Queue>
void Monitor(const Timestamp& start_ts, const SolverOptions& options, const Level* level, const StateMap<State>& states, const Queue& queue, DeadlockDB<typename State::Boxes>& deadlock_db, vector<Counters>& counters) {
    Corrals<State> corrals(level);
    bool running = options.verbosity > 0 && options.monitor;
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

        print("elapsed {} ", seconds);
        q.print();
        print("deadlock_db [{}]", deadlock_db.monitor());
        print(" states [{}]", states.monitor());
        print(" queue [{}]", queue.monitor());
        print("\n");
        if (options.verbosity < 2) continue;

        while (true) {
            auto ss = queue.top();
            if (!ss.has_value()) break;
            State s = ss.value();

            if (deadlock_db.is_complex_deadlock(s.agent, s.boxes, q)) {
                if (!queue.wait_while_running_for(10ms)) return;
                continue;
            }

            int shard = StateMap<State>::shard(s);
            states.lock(shard);
            const StateInfo* si = states.query(s, shard);
            states.unlock(shard);

            int priority = int(si->distance) * options.dist_w + int(si->heuristic) * options.heur_w;
            print("distance {}, heuristic {}, priority {}\n", si->distance, si->heuristic, priority);
            corrals.find_unsolved_picorral(s);
            PrintWithCorral(level, s, corrals.opt_picorral());

            /*for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                const Cell* c = b->dir(d);
                if (corrals.has_picorral() && !corrals.picorral()[c->id]) return;
                State ns(b->id, s.boxes);
                ns.boxes.reset(b->id);
                ns.boxes.set(c->id);
                if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, q)) return;
                print("child: dist {}, heur {}\n", si->distance + 1, heuristic(level, ns.boxes));
                Print(level, ns);
            });*/
            break;
        }
    }
}

template <typename State>
class StateQueue {
    struct T {
        typename State::Boxes boxes;
        Agent agent;
        float priority;
    };

public:
    StateQueue() : _queue([](const T& a, const T& b) { return b.priority < a.priority; }) { // TODO tie breaker if same priority
    }

    void push(State s, float priority) {
        unique_lock lock(_push_mutex);
        _queue.push({std::move(s.boxes), s.agent, priority});
    }

    optional<State> top() const {
        if (_queue.empty()) return nullopt;
        return State(_queue.top().agent, std::move(_queue.top().boxes));
    }

    optional<State> pop() {
        if (_queue.empty()) return nullopt;
        ON_SCOPE_EXIT(_queue.pop());
        return State(_queue.top().agent, std::move(_queue.top().boxes));
    }

    size_t size() const { return _queue.size(); }

    string monitor() const { return format(""); }

    void shutdown() {
        unique_lock<mutex> lk(_running_lock);
        _running = true;
        _running_cv.notify_all();
    }

    template <class Rep, class Period>
    bool wait_while_running_for(const std::chrono::duration<Rep, Period>& rel_time) const {
        unique_lock<mutex> lk(_running_lock);
        if (_running) _running_cv.wait_for(lk, rel_time);
        return _running;
    }

private:
    mutable mutex _running_lock;
    mutable condition_variable _running_cv;
    mutable bool _running = true;

    mutex _push_mutex;
    priority_queue<T, vector<T>, function<bool(const T&, const T&)>> _queue;
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
    const SolverOptions options;

    const Level* level;
    StateMap<State> states;
    ConcurrentStateQueue<State> queue;
    vector<Counters> counters;
    Boxes goals;
    DeadlockDB<Boxes> deadlock_db;

    Solver(const Level* level, const SolverOptions& options)
            : concurrency(options.single_thread ? 1 : thread::hardware_concurrency())
            , options(options)
            , level(level)
            , queue(concurrency)
            , deadlock_db(level) {
        for (Cell* c : level->goals()) goals.set(c->id);
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
        if (ws.corrals.has_picorral() && !ws.corrals.picorral()[c->id]) {
            q.corral_cuts += 1;
            return true;
        }
        State ns(b->id, s.boxes);
        ns.boxes.reset(b->id);
        ns.boxes.set(c->id);

        if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, q)) return false;

        Timestamp norm_ts;
        normalize(level, &ns.agent, ns.boxes);

        Timestamp states_query_ts;
        q.norm_ticks += norm_ts.elapsed(states_query_ts);

        int shard = StateMap<State>::shard(ns);
        states.lock(shard);

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
                qs->prev_agent = b->dir(d ^ 2)->id;
                states.unlock(shard);
                queue.push(ns, uint(qs->distance) * options.dist_w + uint(qs->heuristic) * options.heur_w);
                q.updates += 1;
            }
            q.state_ticks += states_query_ts.elapsed();
            return true;
        }

        // new state
        StateInfo nsi;
        nsi.dir = d;
        nsi.distance = si.distance + 1;

        Timestamp heuristic_ts;
        q.state_ticks += states_query_ts.elapsed(heuristic_ts);

        // TODO holding lock while computing heuristic!
        uint h = heuristic(level, ns.boxes);
        q.heuristic_ticks += heuristic_ts.elapsed();

        if (h == Cell::Inf) {
            states.unlock(shard);
            q.heuristic_deadlocks += 1;
            deadlock_db.add_deadlock(ns.agent, ns.boxes);
            return false;
        }
        if (h > std::numeric_limits<decltype(nsi.heuristic)>::max()) THROW(runtime_error, "heuristic overflow {}", h);
        nsi.heuristic = h;

        nsi.prev_agent = b->dir(d ^ 2)->id;

        Timestamp state_insert_ts;
        states.add(ns, nsi, shard);
        states.unlock(shard);

        Timestamp queue_push_ts;
        q.state_insert_ticks += state_insert_ts.elapsed(queue_push_ts);

        queue.push(ns, int(nsi.distance) * options.dist_w + int(nsi.heuristic) * options.heur_w);
        q.queue_ticks += queue_push_ts.elapsed();

        if (options.debug) {
            print("child:\n");
            Print(level, ns.agent, ns.boxes);
        }

        if (goals.contains(ns.boxes)) {
            queue.shutdown();
            auto& result = *ws.result;
            unique_lock<mutex> lock(result._mutex);
            if (!result._data.has_value()) result._data = pair<State, StateInfo>{ns, nsi};
        }
        return true;
    }

    optional<pair<State, StateInfo>> Solve(State start, bool pre_normalize = true) {
        if (concurrency == 1) print(warning, "Warning: Single-threaded!\n");
        Timestamp start_ts;
        optional<Timestamp> end_ts;
        if (options.max_time != 0) end_ts = Timestamp(start_ts.ticks() + ulong(options.max_time / Timestamp::ms_per_tick() * 1000));
        atomic<bool> timed_out = false;

        if (pre_normalize) normalize(level, &start.agent, start.boxes);
        states.add(start, StateInfo(), StateMap<State>::shard(start));
        queue.push(start, 0);

        if (start.boxes == goals) return pair<State, StateInfo>{start, StateInfo()};

        Protected<optional<pair<State, StateInfo>>> result;

        counters.resize(thread::hardware_concurrency());
        thread monitor([this, start_ts]() { Monitor(start_ts, options, level, states, queue, deadlock_db, counters); });

        parallel(concurrency, [&](size_t thread_id) {
            Counters& q = counters[thread_id];
            WorkerState ws(level);
            ws.result = &result;
            ws.counters = &q;

            while (true) {
                Timestamp queue_pop_ts;
                ON_SCOPE_EXIT(q.total_ticks += queue_pop_ts.elapsed());

                if (end_ts.has_value() && Timestamp().ticks() >= end_ts->ticks()) {
                    queue.shutdown();
                    timed_out = true;
                    break;
                }
                auto p = queue_pop();
                if (!p) return;
                const State& s = p->first;
                if (deadlock_db.is_complex_deadlock(s.agent, s.boxes, q)) continue;
                const StateInfo& si = p->second;

                if (options.debug) {
                    print("popped:\n");
                    Print(level, s.agent, s.boxes);
                }

                // TODO heuristic: order goals somehow (corner goals first) and try to find solution in goal order
                // -> if that doesn't work then do a regular search

                Timestamp corral_ts;
                q.queue_ticks += queue_pop_ts.elapsed(corral_ts);
                ws.corrals.find_unsolved_picorral(s);
                q.corral_ticks += corral_ts.elapsed();

                bool deadlock = true;
                for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                    if (EvaluatePush(s, si, a, b, d, ws)) deadlock = false;
                });
                if (deadlock) deadlock_db.add_deadlock(s.agent, s.boxes);
            }
        });
        monitor.join();
        if (timed_out) print(warning, "Out of time!\n");
        return result._data;
    }
};

#if 0
template <typename State>
struct AltSolver {
    using Boxes = typename State::Boxes;

    const int concurrency;
    const SolverOptions options;

    const Level* level;
    StateMap<State> states;
    StateQueue<State> queue;
    vector<Counters> counters;
    Boxes goals;
    DeadlockDB<Boxes> deadlock_db;
    ctpl::thread_pool pool;

    AltSolver(const Level* level, const SolverOptions& options)
            : concurrency(options.single_thread ? 1 : thread::hardware_concurrency())
            , options(options)
            , level(level)
            , deadlock_db(level)
            , pool(concurrency) {
        for (Cell* c : level->goals()) goals.set(c->id);
    }

    optional<pair<State, StateInfo>> queue_pop() {
        optional<State> s = queue.pop();
        if (!s) return nullopt;

        int shard = StateMap<State>::shard(*s);
        states.lock2(shard);
        StateInfo* q = states.query(*s, shard);
        if (!q || q->closed) THROW(runtime_error, "queue pop failure");
        StateInfo si = *q;  // copy before unlock
        q->closed = true;
        states.unlock(shard);
        return pair<State, StateInfo>{*s, si};
    }

    // Returns false is push is a deadlock.
    bool EvaluatePush(const State& s, const StateInfo& si, const Corrals<State>& corrals, const Cell* a, const Cell* b, const int d, Counters& q, Protected<optional<pair<State, StateInfo>>>& result) {
        const Cell* c = b->dir(d);
        if (corrals.has_picorral() && !corrals.picorral()[c->id]) {
            q.corral_cuts += 1;
            return true;
        }
        State ns(b->id, s.boxes);
        ns.boxes.reset(b->id);
        ns.boxes.set(c->id);

        if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, q)) return false;

        Timestamp norm_ts;
        normalize(level, ns);

        Timestamp states_query_ts;
        q.norm_ticks += norm_ts.elapsed(states_query_ts);

        int shard = StateMap<State>::shard(ns);
        states.lock(shard);

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
                qs->prev_agent = b->dir(d ^ 2)->id;
                states.unlock(shard);
                queue.push(ns, uint(qs->distance) * options.dist_w + uint(qs->heuristic) * options.heur_w);
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
        uint h = heuristic(level, ns.boxes);
        q.heuristic_ticks += heuristic_ts.elapsed();

        if (h == Cell::Inf) {
            states.unlock(shard);
            q.heuristic_deadlocks += 1;
            deadlock_db.add_deadlock(ns.agent, ns.boxes);
            return false;
        }
        if (h > std::numeric_limits<decltype(nsi.heuristic)>::max()) THROW(runtime_error, "heuristic overflow {}", h);
        nsi.heuristic = h;

        nsi.prev_agent = b->dir(d ^ 2)->id;

        Timestamp state_insert_ts;
        states.add(ns, nsi, shard);
        states.unlock(shard);

        Timestamp queue_push_ts;
        q.state_insert_ticks += state_insert_ts.elapsed(queue_push_ts);

        queue.push(ns, int(nsi.distance) * options.dist_w + int(nsi.heuristic) * options.heur_w);
        q.queue_push_ticks += queue_push_ts.elapsed();

        if (goals.contains(ns.boxes)) {
            unique_lock<mutex> lock(result._mutex);
            if (!result._data.has_value()) result._data = pair<State, StateInfo>{ns, nsi};
        }
        return true;
    }

    optional<pair<State, StateInfo>> Solve(State start, bool pre_normalize = true) {
        if (concurrency == 1) print(warning, "Warning: Single-threaded!\n");
        Timestamp start_ts;
        optional<Timestamp> end_ts;
        if (options.max_time != 0) end_ts = Timestamp(start_ts.ticks() + ulong(options.max_time / Timestamp::ms_per_tick() * 1000));

        if (pre_normalize) normalize(level, start);
        states.add(start, StateInfo(), StateMap<State>::shard(start));
        queue.push(start, 0);

        if (start.boxes == goals) return pair<State, StateInfo>{start, StateInfo()};

        Protected<optional<pair<State, StateInfo>>> result;

        counters.resize(thread::hardware_concurrency());
        thread monitor([this, start_ts]() { Monitor(start_ts, options, level, states, queue, deadlock_db, counters); });

        Corrals<State> corrals(level);
        vector<future<bool>> results;
        while (true) {
            Timestamp queue_pop_ts;
            Counters q; // TODO fixme
            ON_SCOPE_EXIT(q.total_ticks += queue_pop_ts.elapsed());

            if (end_ts.has_value() && Timestamp().ticks() >= end_ts->ticks()) {
                print(warning, "Out of time!\n");
                break;
            }

            auto p = queue_pop();
            if (!p) return nullopt;
            const State& s = p->first;
            if (deadlock_db.is_complex_deadlock(s.agent, s.boxes, q)) continue;
            const StateInfo& si = p->second;

            // TODO heuristic: order goals somehow (corner goals first) and try to find solution in goal order
            // -> if that doesn't work then do a regular search

            Timestamp corral_ts;
            q.queue_pop_ticks += queue_pop_ts.elapsed(corral_ts);
            corrals.find_unsolved_picorral(s);
            q.corral_ticks += corral_ts.elapsed();

            results.clear();

            for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                results.push_back(pool.push([&](int id) { return EvaluatePush(s, si, corrals, a, b, d, q, result); }));
            });

            bool deadlock = true;
            for (auto& result : results) if (result.get()) deadlock = false;
            if (deadlock) deadlock_db.add_deadlock(s.agent, s.boxes);
        }
        monitor.join();
        return result._data;
    }
};
#endif

template <typename Boxes>
Solution InternalSolve(const Level* level, const SolverOptions& options) {
    if (options.verbosity > 0) PrintInfo(level);

    if (false && options.alt) {
#if 0
        AltSolver<TState<Boxes>> solver(level, options);
        auto solution = solver.Solve(level->start);
        if (solution) return ExtractSolution(*solution, level, solver.states);
#endif
    } else {
        Solver<TState<Boxes>> solver(level, options);
        auto solution = solver.Solve(TState(level->start_agent, level->start_boxes));
        if (solution) return ExtractSolution(*solution, level, solver.states);
    }
    return {};
}

Solution Solve(const Level* level, const SolverOptions& options) {
#define DENSE(N) \
    if (level->num_alive <= 32 * N) { print("Using DenseBoxes<{}>\n", N); return InternalSolve<DenseBoxes<N>>(level, options); }

    DENSE(1);
    DENSE(2);
    DENSE(3);
    DENSE(4);
//    DENSE(5);
//    DENSE(6);
//    DENSE(7);
//    DENSE(8);
#undef DENSE

    print(warning, "Warning: Using DynamicBoxes\n");
    return InternalSolve<DynamicBoxes>(level, options);
}

template<typename Boxes>
vector<const Cell*> ShortestPath(const Level* level, const Cell* start, const Cell* end, const Boxes& boxes) {
    if (boxes[start->id] || boxes[end->id]) THROW(runtime_error, "precondition");

    vector<const Cell*> prev(level->cells.size());
    AgentVisitor visitor(start);
    for (const Cell* a : visitor) {
        // Uses moves instead of actions, as it needs to reconstruct the entire move path
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
pair<vector<int2>, int> Solve(LevelEnv env, const SolverOptions& options) {
    auto level = LoadLevel(env);
    ON_SCOPE_EXIT(Destroy(level));
    auto pushes = Solve(level, options);
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
    return {std::move(steps), pushes.size()};
}

template <typename Boxes>
void add_more_boxes(const Level* level, Boxes& boxes, const Boxes& goals, int num_boxes, int b0, vector<Boxes>& all_boxes) {
    if (num_boxes == 0) {
        if (!goals.contains(boxes)) all_boxes.push_back(boxes);
        return;
    }

    for (uint b = b0; b < level->num_alive; b++) {
        boxes.set(b);
        if (!is_simple_deadlock(level->cells[b], boxes))
            add_more_boxes(level, boxes, goals, num_boxes - 1, b + 1, all_boxes);
        boxes.reset(b);
    }
}

// TODO move to utils
template <typename State>
bool contains_deadlock(const Level* level, const State& s, const State& deadlock) {
    return s.boxes.contains(deadlock.boxes) &&
           is_cell_reachable(level->cells[s.agent], level->cells[deadlock.agent], deadlock.boxes);
}

// TODO move to utils
template <typename State>
bool contains_deadlock(const Level* level, const State& s, const vector<State>& deadlocks) {
    for (const State& d : deadlocks)
        if (contains_deadlock(level, s, d)) return true;
    return false;
}

template <typename State>
void generate_deadlocks(const Level* level, const SolverOptions& options, int num_boxes, vector<State>& deadlocks) {
    vector<typename State::Boxes> all_boxes;
    typename State::Boxes boxes, goals;
    for (Cell* c : level->goals()) goals.set(c->id);
    add_more_boxes(level, boxes, goals, num_boxes, 0, all_boxes);

    vector<pair<State, int>> candidates;
    State s;
    for (const auto& my_boxes : all_boxes) {
        AgentVisitor visitor(level);
        int h = heuristic(level, my_boxes);
        for (uint b = 0; b < level->num_alive; b++)
            if (my_boxes[b])
                for (const Cell* a : level->cells[b]->new_moves)
                    if (!my_boxes[a->id] && !visitor.visited(a)) {
                        // visit everything reachable from A
                        s.boxes = my_boxes;
                        s.agent = a->id;

                        // Normalize s (without clearing visitor)
                        visitor.add(a);
                        for (const Cell* ea : visitor) {
                            if (ea->id < s.agent) s.agent = ea->id;
                            for (const Cell* eb : ea->new_moves) {
                                if (!s.boxes[eb->id]) visitor.add(eb);
                            }
                        }

                        for (const Cell* c : a->new_moves)
                            if (!my_boxes[c->id]) {
                                candidates.emplace_back(s, h);
                                break;
                            }
                    }
    }

    // order candidates by highest heuristic value first to maximize overlaps in paths
    sort(candidates, [](const pair<State, int>& a, const pair<State, int>& b) { return a.second > b.second; });

    Solver<State> solver(level, options);
    vector<State> new_deadlocks;
    absl::flat_hash_set<State> solvable;
    for (auto [candidate, _] : candidates)
        if (!solvable.contains(candidate) && !contains_deadlock(level, candidate, deadlocks)) {
            solver.states.reset();
            solver.queue.reset();
            auto result = solver.Solve(candidate, false /*pre_normalize*/);
            if (result.has_value()) {
                for (const State& p : ExtractSolution(*result, level, solver.states)) {
                    solvable.insert(p);
                }
            } else {
                Print(level, candidate.agent, candidate.boxes);
                new_deadlocks.push_back(candidate);
            }
        }
    deadlocks.reserve(deadlocks.size() + new_deadlocks.size());
    std::copy(new_deadlocks.begin(), new_deadlocks.end(), std::back_inserter(deadlocks));
}

template <typename Cell>
void InternalGenerateDeadlocks(const Level* level, const SolverOptions& options) {
    const int MaxBoxes = 4;
    Timestamp ts;
    using State = TState<DynamicBoxes>;
    // using State = TState<SparseBoxes<Cell, MaxBoxes>>;
    vector<State> deadlocks;
    for (auto boxes : range(1, MaxBoxes + 1)) generate_deadlocks(level, options, boxes, deadlocks);
    print("found deadlocks {} in {}\n", deadlocks.size(), ts.elapsed());
}

void GenerateDeadlocks(const Level* level, const SolverOptions& options) {
    if (level->num_alive < 256)
        InternalGenerateDeadlocks<uchar>(level, options);
    else
        InternalGenerateDeadlocks<ushort>(level, options);
}
