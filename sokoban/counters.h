#pragma once
#include "core/fmt.h"
#include "core/timestamp.h"

inline double Sec(ulong ticks) { return Timestamp::to_s(ticks); }

struct Counters {
    typedef ulong T;

    T simple_deadlocks = 0;
    T db_deadlocks = 0;
    T frozen_box_deadlocks = 0;
    T heuristic_deadlocks = 0;
    T bipartite_deadlocks = 0;
    T corral_cuts = 0;
    T duplicates = 0;
    T updates = 0;

    // main ticks
    T queue_pop_ticks = 0;
    T corral_ticks = 0;
    T corral2_ticks = 0;
    T is_simple_deadlock_ticks = 0;
    T db_contains_pattern_ticks = 0;
    T contains_frozen_boxes_ticks = 0;
    T pattern_matches_ticks = 0;
    T bipartite_ticks = 0;
    T norm_ticks = 0;
    T states_query_ticks = 0;
    T heuristic_ticks = 0;
    T state_insert_ticks = 0;
    T queue_push_ticks = 0;
    T features_ticks = 0;
    T pattern_add_ticks = 0;
    T contains_box_blocked_goals_ticks = 0;

    T total_ticks = 0;

    Counters() { memset(this, 0, sizeof(Counters)); }

    ulong else_ticks() const {
        ulong a = total_ticks;
        a -= queue_pop_ticks;
        a -= corral_ticks;
        a -= corral2_ticks;
        a -= is_simple_deadlock_ticks;
        a -= db_contains_pattern_ticks;
        a -= contains_frozen_boxes_ticks;
        a -= pattern_matches_ticks;
        a -= bipartite_ticks;
        a -= norm_ticks;
        a -= states_query_ticks;
        a -= heuristic_ticks;
        a -= state_insert_ticks;
        a -= queue_push_ticks;
        a -= features_ticks;
        a -= pattern_add_ticks;
        a -= contains_box_blocked_goals_ticks;
        return a;
    }

    void tick(string_view name, T count, bool* first) const {
        if (count == 0) return;
        if (!*first) ::print(", ");
        ::print("{} {:.1f}", name, (count * 100.0) / total_ticks);
        *first = false;
    }

    void print() const {
        bool first = true;
        tick("queue_pop", queue_pop_ticks, &first);
        tick("corral", corral_ticks, &first);
        tick("corral2", corral2_ticks, &first);
        tick("is_simple_deadlock", is_simple_deadlock_ticks, &first);
        tick("db_contains_pattern", db_contains_pattern_ticks, &first);
        tick("contains_frozen_boxes", contains_frozen_boxes_ticks, &first);
        tick("pattern_matches", pattern_matches_ticks, &first);
        tick("bipartite", bipartite_ticks, &first);
        tick("norm", norm_ticks, &first);
        tick("states_query", states_query_ticks, &first);
        tick("heuristic", heuristic_ticks, &first);
        tick("state_insert", state_insert_ticks, &first);
        tick("queue_push", queue_push_ticks, &first);
        tick("features", features_ticks, &first);
        tick("pattern_add", pattern_add_ticks, &first);
        tick("contains_box_blocked_goals", contains_box_blocked_goals_ticks, &first);
        tick("else", else_ticks(), &first);

        ::print("\ndeadlocks (simple {}, db {}, frozen_box {}, bipartite {}, heuristic {})", simple_deadlocks, db_deadlocks, frozen_box_deadlocks, bipartite_deadlocks, heuristic_deadlocks);
        ::print(", corral cuts {}, dups {}, updates {}\n", corral_cuts, duplicates, updates);
    }

    void add(const Counters& src) {
        const T* s = reinterpret_cast<const T*>(&src);
        const T* e = s + sizeof(Counters) / sizeof(T);
        static_assert(sizeof(Counters) % sizeof(T) == 0);
        T* d = reinterpret_cast<T*>(this);
        while (s != e) *d++ += *s++;
    }
};
