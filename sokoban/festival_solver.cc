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
#include "sokoban/util.h"

#include <queue>

#include "absl/container/flat_hash_map.h"

using namespace std::chrono_literals;
using std::nullopt;
using std::vector;
using std::array;
using std::pair;
using std::map;
using std::optional;
using absl::flat_hash_map;

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

ushort ComputePacking(const State& state) {
    // TODO
    return 0;
}

ushort ComputeConnectivity(const State& state) {
    // TODO
    return 0;
}

ushort ComputeRoomConnectivity(const State& state) {
    // TODO
    return 0;
}

ushort ComputeOutOfPlan(const State& state) {
    // TODO
    return 0;
}

Features ComputeFeatures(const State& state) {
    Features features;
    features.packing = ComputePacking(state);
    features.connectivity = ComputeConnectivity(state);
    features.room_connectivity = ComputeRoomConnectivity(state);
    features.out_of_plan = ComputeOutOfPlan(state);
    return features;
}

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
        closed_states.emplace(start, Closed());
        fs_queues[ComputeFeatures(start)].push({.state = start, .distance = 0});

        Corrals<State> corrals(level);

        while (true) {
            bool pushed = false;
            for (auto& [features, queue] : fs_queues) {
                if (queue.empty()) continue;
                Queued queued = queue.top();
                queue.pop();

                const State& s = queued.state;
                if (closed_states.contains(s)) continue;
                closed_states.emplace(s, Closed{.prev = queued.prev, .distance = queued.distance});

                corrals.find_unsolved_picorral(s);
                for_each_push(level, s, [&](const Cell* a, const Cell* b, int d) {
                    const Cell* c = b->dir(d);
                    if (corrals.has_picorral() && !corrals.picorral()[c->id]) return;

                    State ns(b->id, s.boxes);
                    ns.boxes.reset(b->id);
                    ns.boxes.set(c->id);
                    normalize(level, ns);

                    if (closed_states.contains(ns)) return;
                    if (deadlock_db.is_deadlock(ns.agent, ns.boxes, c, counters)) return;

                    auto& nq = fs_queues[ComputeFeatures(ns)];
                    nq.push({.state = std::move(ns), .prev = s, .distance = ushort(queued.distance + 1)});
                    pushed = true;
                });
            }
            if (!pushed) return false;
        }
    }

};

// Returns steps as deltas and number of pushes.
pair<vector<int2>, int> FestivalSolve(LevelEnv env, const SolverOptions& options) {
    auto level = LoadLevel(env);
    ON_SCOPE_EXIT(Destroy(level));
    FestivalSolver solver(level, options);
    print(warning, "Level solved {}\n", solver.Solve(level->start));
    return {};
}
