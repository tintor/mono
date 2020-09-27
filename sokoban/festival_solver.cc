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
using min_priority_queue = priority_queue<T, vector<T>, std::greater<T>>;

using Boxes = DenseBoxes<4>;
using State = TState<Boxes>;

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

// Number of boxes on goals in goal_penalty order.
ushort ComputePacking(const Cell* agent, const Boxes& boxes) {
    ushort count = 0;
    for (const Cell* g : agent->level->goals_in_packing_order) {
        if (boxes[g->id]) {
            count += 1;
        } else {
            break;
        }
    }
    return count;
}

// Number of separate areas between boxes.
ushort ComputeConnectivity(const Cell* agent, const Boxes& boxes) {
    ushort count = 0;
    AgentVisitor visitor(agent->level);
    // TODO mark all boxes as visited for perf
    for (const Cell* v : agent->level->cells) {
        if (!visitor.visited(v) && !boxes[v]) {
            count += 1;
            visitor.add(v);
            for (const Cell* a : visitor) {
                for (auto [_, b] : a->moves) {
                    if (!boxes[b]) visitor.add(b);
                }
            }
        }
    }
    return count;
}

// TODO precompute in level
bool IsGate(const Cell* a) {
    for (int d = 0; d < 4; d++) {
        const Cell* b = a->dir(d);
        if (b && (!a->dir(d + 1) || !b->dir(d + 1)) && (!a->dir(d - 1) || !b->dir(d - 1))) return true;
    }
    // Two diagonal cases:
    if (a->moves.size() >= 3) {
        if (!a->dir8[4] && !a->dir8[7]) return true;
        if (!a->dir8[5] && !a->dir8[6]) return true;
    }
    return false;
}

// How many boxes are in a gate? (ie. blocking movement between two areas)
ushort ComputeRoomConnectivity(const Cell* agent, const Boxes& boxes) {
    ushort count = 0;
    for (const Cell* a : agent->level->alive()) {
        if (boxes[a] && IsGate(a)) {
            count += 1;
        }
    }
    return count;
}

// If next box in packing order were to be placed next, how many non-goal boxes would be unreachable?
ushort ComputeOutOfPlan(const Cell* agent, const Boxes& boxes) {
    return 0;
    const Cell* next_goal = nullptr;
    for (const Cell* g : agent->level->goals_in_packing_order) {
        if (!boxes[g->id]) {
            next_goal = g;
            break;
        }
    }

    AgentVisitor reachable(agent);
    for (const Cell* a : reachable) {
        for (auto [_, b] : a->moves) {
            if (!boxes[b->id] && b != next_goal) reachable.add(b);
        }
    }

    ushort count = 0;
    for (const Cell* a : agent->level->alive()) {
        if (reachable.visited(a) || a == next_goal) continue;
        if ((boxes[a] && !a->goal) || (!boxes[a] && a->goal)) count += 1;
    }
    return count;
}

void PrintOutOfPlan(const Cell* agent, const Boxes& boxes) {
    const Cell* next_goal = nullptr;
    for (const Cell* g : agent->level->goals_in_packing_order) {
        if (!boxes[g->id]) {
            next_goal = g;
            break;
        }
    }

    AgentVisitor reachable(agent);
    for (const Cell* a : reachable) {
        for (auto [_, b] : a->moves) {
            if (!boxes[b->id] && b != next_goal) reachable.add(b);
        }
    }

    string str;
    Print(agent->level, agent->id, boxes, [&](const Cell* e) -> string_view {
        if (e == next_goal) return " @";
        if (!e->alive || reachable.visited(e) || e == next_goal) return "";
        if ((boxes[e] && !e->goal) || (!boxes[e] && e->goal)) return " x";
        return "";
    });
}

Features ComputeFeatures(const Cell* agent, const Boxes& boxes) {
    Features features;
    features.packing = ComputePacking(agent, boxes);
    features.connectivity = ComputeConnectivity(agent, boxes);
    features.room_connectivity = ComputeRoomConnectivity(agent, boxes);
    features.out_of_plan = ComputeOutOfPlan(agent, boxes);
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
                Print(level, ns.agent, ns.boxes);
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
    map<Features, min_priority_queue<Queued>> fs_queues;  // using map for stable iterators
    Counters counters;
    Boxes goals;
    DeadlockDB<Boxes> deadlock_db;

    FestivalSolver(const Level* level, const SolverOptions& options)
            : options(options)
            , level(level)
            , deadlock_db(level) {
        for (Cell* c : level->goals()) goals.set(c->id);
    }

    bool Solve(Agent start_agent, const Boxes& start_boxes) {
        if (start_boxes == goals) return true;
        Timestamp start_ts;
        optional<Timestamp> end_ts;
        if (options.max_time != 0) end_ts = Timestamp(start_ts.ticks() + ulong(options.max_time / Timestamp::ms_per_tick() * 1000));

        normalize(level, &start_agent, start_boxes);
        fs_queues[ComputeFeatures(level->cells[start_agent], start_boxes)].push({.state = State(start_agent, start_boxes), .distance = 0});

        //thread monitor([this, start_ts]() { Monitor(start_ts, options, level, closed_states, fs_queues, deadlock_db, counters); });
        Corrals<State> corrals(level);

        Timestamp prev_ts;
        while (true) {
            if (prev_ts.elapsed_s() >= 5) {
                size_t open = 0;
                for (const auto& [_, queue] : fs_queues) open += queue.size();
                print("elapsed {:.0f}, closed {}, open {}, queues {}\n", start_ts.elapsed_s(), closed_states.size(), open, fs_queues.size());
                counters.print();
                /*for (const auto& [features, queue] : fs_queues) {
                    print("features {}, queue {}\n", features.summary(), queue.size());
                    if ((rand() % 40 == 0 || features.connectivity == 1) && !queue.empty()) {
                        const State& s = queue.top().state;
                        Print(level, s.agent, s.boxes);
                    }
                }*/
                print("\n");
                prev_ts = Timestamp();
            }

            bool popped = false;
            for (auto it = fs_queues.begin(); it != fs_queues.end();) {
                auto& queue = it->second;
                if (queue.empty()) { it = fs_queues.erase(it); continue; }

                Queued queued = queue.top();
                queue.pop();
                popped = true;

                const State& s = queued.state;
                if (closed_states.contains(s)) { it++; continue; }
                closed_states.emplace(s, Closed{.prev = queued.prev, .distance = queued.distance});

                if (goals.contains(s.boxes)) return true;
                if (options.debug) {
                    print("popped:\n");
                    Print(level, s.agent, s.boxes);
                }

                corrals.find_unsolved_picorral(s);
                for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                    const Cell* c = b->dir(d);
                    if (corrals.has_picorral() && !corrals.picorral()[c->id]) return;

                    State ns(b->id, s.boxes);
                    ns.boxes.move(b, c);
                    normalize(level, &ns.agent, ns.boxes);

                    if (closed_states.contains(ns)) return;
                    if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, counters)) return;

                    auto& nq = fs_queues[ComputeFeatures(level->cells[ns.agent], ns.boxes)];
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
    if (!solver.Solve(level->start_agent, level->start_boxes)) return {};
    int2 a = {0, 0};
    return {{a}, 1}; // TODO
}
