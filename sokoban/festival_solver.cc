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
    State prev; // in starting state: prev == state
    ushort distance = 0;
};

struct Queued {
    State state;
    State prev; // in starting state: prev == state
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

struct FestivalSolver {
    const SolverOptions options;

    const Level* level;
    flat_hash_map<State, Closed> closed_states;
    map<Features, min_priority_queue<Queued>> fs_queues;  // using map for fast iteration
    Counters counters;
    Boxes goals;
    DeadlockDB<Boxes> deadlock_db;

    FestivalSolver(const Level* level, const SolverOptions& options)
            : options(options)
            , level(level)
            , deadlock_db(level) {
        for (Cell* c : level->goals()) goals.set(c->id);
    }

    void Enqueue(Queued queued) {
        Features features = TIMER(ComputeFeatures(level->cells[queued.state.agent], queued.state.boxes), counters.features_ticks);
        TIMER(fs_queues[features].push(std::move(queued)), counters.queue_push_ticks);
    }

    ulong NumOpen() const {
        ulong open = 0;
        for (const auto& [_, queue] : fs_queues) open += queue.size();
        return open;
    }

    bool Solve(Agent start_agent, const Boxes& start_boxes) {
        if (start_boxes == goals) return true;
        Timestamp start_ts;
        optional<Timestamp> end_ts;
        if (options.max_time != 0) end_ts = Timestamp(start_ts.ticks() + ulong(options.max_time / Timestamp::ms_per_tick() * 1000));

        normalize(level, &start_agent, start_boxes);
        Enqueue({.state = State(start_agent, start_boxes), .distance = 0});

        Corrals<State> corrals(level);

        Timestamp prev_ts;
        while (!fs_queues.empty()) {
            if (prev_ts.elapsed_s() >= 5) {
                counters.total_ticks += prev_ts.elapsed();

                print("elapsed {:.0f}, closed {}, open {}, queues {}\n", start_ts.elapsed_s(), closed_states.size(), NumOpen(), fs_queues.size());
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

            for (auto it = fs_queues.begin(); it != fs_queues.end();) {
                auto& queue = it->second;
                if (queue.empty()) { it = fs_queues.erase(it); continue; }

                Queued queued = queue.top();
                TIMER(queue.pop(), counters.queue_pop_ticks);

                const State& s = queued.state;
                if (closed_states.contains(s)) { it++; continue; }
                TIMER(closed_states.emplace(s, Closed{.prev = queued.prev, .distance = queued.distance}), counters.state_insert_ticks);

                if (goals.contains(s.boxes)) {
                    counters.total_ticks += prev_ts.elapsed();
                    print("elapsed {:.0f}, closed {}, open {}, queues {}\n", start_ts.elapsed_s(), closed_states.size(), NumOpen(), fs_queues.size());
                    counters.print();
                    return true;
                }
                if (options.debug) {
                    print("popped:\n");
                    Print(level, s.agent, s.boxes);
                }

                corrals.find_unsolved_picorral(s);
                for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                    const Cell* c = b->dir(d);
                    if (corrals.has_picorral() && !corrals.picorral()[c->id]) { counters.corral_cuts += 1; return; }

                    State ns(b->id, s.boxes);
                    ns.boxes.move(b, c);
                    TIMER(normalize(level, &ns.agent, ns.boxes), counters.norm_ticks);

                    if (closed_states.contains(ns)) { counters.duplicates += 1; return; }
                    if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, counters)) return;

                    Enqueue({.state = std::move(ns), .prev = s, .distance = ushort(queued.distance + 1)});
                });

            }
        }
        return false;
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
