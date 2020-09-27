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

    T fb1 = 0;
    T fb2 = 0;
    T fb3 = 0;
    T fb4 = 0;
    T fb5 = 0;
    T fb6 = 0;

    T fb1a_ticks = 0;
    T fb1b_ticks = 0;

    // main ticks
    T queue_pop_ticks = 0;
    T corral_ticks = 0;
    T corral2_ticks = 0;
    T is_simple_deadlock_ticks = 0;
    T db_contains_pattern_ticks = 0;
    T contains_frozen_boxes_ticks = 0;
    T bipartite_ticks = 0;
    T norm_ticks = 0;
    T states_query_ticks = 0;
    T heuristic_ticks = 0;
    T state_insert_ticks = 0;
    T queue_push_ticks = 0;
    T features_ticks = 0;

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
        a -= bipartite_ticks;
        a -= norm_ticks;
        a -= states_query_ticks;
        a -= heuristic_ticks;
        a -= state_insert_ticks;
        a -= queue_push_ticks;
        a -= features_ticks;
        return a;
    }

    void tick(string_view name, T count, bool comma = true) const {
        if (count == 0) return;
        if (comma) ::print(", ");
        ::print("{} {:.1f}", name, (count * 100.0) / total_ticks);
    }

    void print() const {
        tick("queue_pop", queue_pop_ticks);
        tick("corral", corral_ticks);
        tick("corral2", corral2_ticks);
        tick("is_simple_deadlock", is_simple_deadlock_ticks);
        tick("db_contains_pattern", db_contains_pattern_ticks);
        tick("contains_frozen_boxes", contains_frozen_boxes_ticks);
        tick("bipartite", bipartite_ticks);
        tick("norm", norm_ticks);
        tick("states_query", states_query_ticks);
        tick("heuristic", heuristic_ticks);
        tick("state_insert", state_insert_ticks);
        tick("queue_push", queue_push_ticks);
        tick("else", else_ticks());

        ::print("\ndeadlocks (simple {}, db {}, frozen_box {}, bipartite {}, heuristic {})", simple_deadlocks, db_deadlocks, frozen_box_deadlocks, bipartite_deadlocks, heuristic_deadlocks);
        ::print(", corral cuts {}, dups {}, updates {}\n", corral_cuts, duplicates, updates);
        ::print("contains_frozen_boxes({} {} {} {} {} {}) {:.1f} {:.1f}\n", fb1, fb2, fb3, fb4, fb5, fb6, Sec(fb1a_ticks), Sec(fb1b_ticks));
    }

    void add(const Counters& src) {
        const T* s = reinterpret_cast<const T*>(&src);
        const T* e = s + sizeof(Counters) / sizeof(T);
        static_assert(sizeof(Counters) % sizeof(T) == 0);
        T* d = reinterpret_cast<T*>(this);
        while (s != e) *d++ += *s++;
    }
};
