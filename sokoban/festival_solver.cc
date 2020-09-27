#include "core/auto.h"
#include "core/bits_util.h"
#include "core/range.h"

#include "sokoban/common.h"
#include "sokoban/solver.h"
#include "sokoban/corrals.h"
#include "sokoban/frozen.h"
#include "sokoban/deadlock.h"
#include "sokoban/util.h"
#include "sokoban/level_loader.h"
#include "sokoban/level_printer.h"

using namespace std::chrono_literals;

template <typename T>
using min_priority_queue = std::priority_queue<T, std::vector<T>, std::greater<T>>;

using Boxes = DenseBoxes<2>;
using State = TState<Boxes>;

/*class StatePriorityQueue {
    struct T {
        Boxes boxes;
        Agent agent;
        float priority;
    };

public:
    StatePriorityQueue() : _queue([](const T& a, const T& b) { return b.priority < a.priority; }) { }

    void push(State s, float priority) {
        _queue.push({std::move(s.boxes), s.agent, priority});
    }

    State pop() {
        if (_queue.empty()) THROW(runtime_error, "queue is empty");
        ON_SCOPE_EXIT(_queue.pop());
        return State(_queue.top().agent, std::move(_queue.top().boxes));
    }

    size_t size() const { return _queue.size(); }

    std::string monitor() const { return format(""); }

private:
    std::priority_queue<T, std::vector<T>, std::function<bool(const T&, const T&)>> _queue;
};*/

struct Features {
    ushort packing;
    ushort connectivity;
    ushort room_connectivity;
    ushort out_of_plan;

    bool operator<(const Features& o) const {
        if (packing != o.packing) return packing < o.packing;
        if (connectivity != o.connectivity) return connectivity < o.connectivity;
        if (room_connectivity != o.room_connectivity) return room_connectivity < o.room_connectivity;
        return out_of_plan < o.out_of_plan;
    }

    string summary() const {
        return format("packing {}, connectivity {}, room_conn {}, out_of_plan {}", packing, connectivity, room_connectivity, out_of_plan);
    }
};

struct Closed {
    optional<State> prev;
    ushort distance = 0;
};

struct Queued {
    State state;
    optional<State> prev;
    ushort distance = 0;

    bool operator==(const Queued& o) const { return distance == o.distance; }
    bool operator>(const Queued& o) const { return distance > o.distance; }
};

ushort ComputePacking(const Level* level, const State& state) {
    // TODO number of boxes on goals in specific order
    ushort count = 0;
    for (const Cell* g : level->goals()) {
        if (state.boxes[g->id]) count += 1;
    }
    return count;
}

// Number of separate areas between boxes.
ushort ComputeConnectivity(const Level* level, const State& state) {
    ushort count = 0;
    AgentVisitor visitor(level);
    // TODO mark all boxes as visited for perf
    for (const Cell* start : level->cells) {
        if (!visitor.visited(start) && !state.boxes[start]) {
            count += 1;
            visitor.add(start);
            for (const Cell* a : visitor) {
                for (auto [_, b] : a->moves) {
                    if (!state.boxes[b]) visitor.add(b);
                }
            }
        }
    }
    return count;
}

ushort ComputeRoomConnectivity(const Level* level, const State& state) {
    // TODO
    return 0;
}

ushort ComputeOutOfPlan(const Level* level, const State& state) {
    // TODO
    return 0;
}

Features ComputeFeatures(const Level* level, const State& state) {
    Features features;
    features.packing = ComputePacking(level, state);
    features.connectivity = ComputeConnectivity(level, state);
    features.room_connectivity = ComputeRoomConnectivity(level, state);
    features.out_of_plan = ComputeOutOfPlan(level, state);
    return features;
}

#if 0
template <typename State, typename Queue>
void Monitor(const Timestamp& start_ts, const SolverOptions& options, const Level* level, const flat_hash_map<State, Closed>& states, const map<Features, min_priority_queue<Queued>>& queues, DeadlockDB<typename State::Boxes>& deadlock_db, vector<Counters>& counters) {
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
        ulong else_ticks = q.total_ticks;
        else_ticks -= q.queue_pop_ticks;
        else_ticks -= q.corral_ticks;
        else_ticks -= q.corral2_ticks;
        else_ticks -= q.is_simple_deadlock_ticks;
        else_ticks -= q.contains_frozen_boxes_ticks;
        else_ticks -= q.norm_ticks;
        else_ticks -= q.states_query_ticks;
        else_ticks -= q.heuristic_ticks;
        else_ticks -= q.state_insert_ticks;
        else_ticks -= q.queue_push_ticks;
        q.else_ticks = else_ticks;

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
#endif

struct FestivalSolver {
    const SolverOptions options;

    const Level* level;
    flat_hash_map<State, Closed> closed_states;
    map<Features, min_priority_queue<Queued>> fs_queues;  // using std::map for stable iterators
    Counters counters;
    Boxes goals;
    DeadlockDB<Boxes> deadlock_db;

    FestivalSolver(const Level* level, const SolverOptions& options)
            : options(options)
            , level(level)
            , deadlock_db(level) {
        for (Cell* c : level->goals()) goals.set(c->id);
    }

    bool Solve(State start) {
        if (start.boxes == goals) return true;
        Timestamp start_ts;
        optional<Timestamp> end_ts;
        if (options.max_time != 0) end_ts = Timestamp(start_ts.ticks() + ulong(options.max_time / Timestamp::ms_per_tick() * 1000));

        normalize(level, start);
        fs_queues[ComputeFeatures(level, start)].push({.state = start, .distance = 0});

        //std::thread monitor([this, start_ts]() { Monitor(start_ts, options, level, closed_states, fs_queues, deadlock_db, counters); });
        Corrals<State> corrals(level);

        Timestamp prev_ts;
        while (true) {
            if (prev_ts.elapsed_s() >= 5) {
                size_t open = 0;
                for (const auto& [_, queue] : fs_queues) open += queue.size();
                print("elapsed {}, closed {}, open {}, queues {}\n", start_ts.elapsed_s(), closed_states.size(), open, fs_queues.size());
                for (const auto& [features, queue] : fs_queues) {
                    if (queue.size() > 0) {
                        print("features {}, queue {}\n", features.summary(), queue.size());
                    }
                }
                prev_ts = Timestamp();
            }

            bool popped = false;
            for (auto& [features, queue] : fs_queues) {
                if (queue.empty()) continue;
                Queued queued = queue.top();
                queue.pop();
                popped = true;

                const State& s = queued.state;
                if (closed_states.contains(s)) continue;
                closed_states.emplace(s, Closed{.prev = queued.prev, .distance = queued.distance});

                if (goals.contains(s.boxes)) return true;
                if (options.debug) {
                    print("popped:\n");
                    Print(level, s);
                }

                corrals.find_unsolved_picorral(s);
                for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                    const Cell* c = b->dir(d);
                    if (corrals.has_picorral() && !corrals.picorral()[c->id]) return;

                    State ns(b->id, s.boxes);
                    ns.boxes.move(b, c);
                    normalize(level, ns);

                    if (closed_states.contains(ns)) return;
                    if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, counters)) return;

                    auto& nq = fs_queues[ComputeFeatures(level, ns)];
                    nq.push({.state = std::move(ns), .prev = s, .distance = ushort(queued.distance + 1)});
                });
            }
            if (!popped) return false;
        }
        //monitor.join();
    }
};

// Returns steps as deltas and number of pushes.
pair<vector<int2>, int> FestivalSolve(LevelEnv env, const SolverOptions& options) {
    auto level = LoadLevel(env);
    if (options.verbosity > 0) PrintInfo(level);
    ON_SCOPE_EXIT(Destroy(level));
    FestivalSolver solver(level, options);
    if (!solver.Solve(level->start)) return {};
    int2 a = {0, 0};
    return {{a}, 1}; // TODO
}
