#pragma once
#include "core/fmt.h"
#include "core/timestamp.h"

double Sec(ulong ticks) { return Timestamp::to_s(ticks); }

struct Counters {
    typedef ulong T;

    T simple_deadlocks = 0;
    T db_deadlocks = 0;
    T frozen_box_deadlocks = 0;
    T heuristic_deadlocks = 0;
    T corral_cuts = 0;
    T duplicates = 0;
    T updates = 0;
    T reversible_pushes = 0;

    T fb1 = 0;
    T fb2 = 0;
    T fb3 = 0;
    T fb4 = 0;
    T fb5 = 0;
    T fb6 = 0;
    T fb7 = 0;

    T fb1a_ticks = 0;
    T fb1b_ticks = 0;

    T total_ticks = 0;
    T queue_pop_ticks = 0;
    T corral_ticks = 0;
    T corral2_ticks = 0;
    T is_simple_deadlock_ticks = 0;
    T is_reversible_push_ticks = 0;
    T db_contains_pattern_ticks = 0;
    T contains_frozen_boxes_ticks = 0;
    T norm_ticks = 0;
    T states_query_ticks = 0;
    T heuristic_ticks = 0;
    T state_insert_ticks = 0;
    T queue_push_ticks = 0;
    T else_ticks = 0;

    Counters() { memset(this, 0, sizeof(Counters)); }

    void print() const {
        ::print("({:.1f} | queue_pop {:.1f}, corral {:.1f} {:.1f}, is_simple_deadlock {:.1f}",
            Sec(total_ticks), Sec(queue_pop_ticks), Sec(corral_ticks), Sec(corral2_ticks), Sec(is_simple_deadlock_ticks));
        ::print(", is_reversible_push {:.1f}, db_contains_pattern {:.1f}, contains_frozen_boxes {:.1f} {:.1f}",
            Sec(is_reversible_push_ticks), Sec(db_contains_pattern_ticks), Sec(contains_frozen_boxes_ticks), Rel(contains_frozen_boxes_ticks));
        ::print(", norm {:.1f}, states_query {:.1f}, heuristic {:.1f}, state_insert {:.1f}, queue_push {:.1f}, else {:.1f})\n",
            Sec(norm_ticks), Sec(states_query_ticks), Sec(heuristic_ticks), Sec(state_insert_ticks), Sec(queue_push_ticks), Sec(else_ticks));
        ::print("deadlocks (simple {}, reversible {}, db {}, frozen_box {}, heuristic {})", simple_deadlocks, reversible_pushes, db_deadlocks, frozen_box_deadlocks, heuristic_deadlocks);
        ::print(", corral cuts {}, dups {}, updates {}\n", corral_cuts, duplicates, updates);
        ::print("contains_frozen_boxes({} {} {} {} {} {} {}) {:.1f} {:.1f}\n", fb1, fb2, fb3, fb4, fb5, fb6, fb7, Sec(fb1a_ticks), Sec(fb1b_ticks));
    }

    double Rel(T a) const {
        return (a * 100.0) / total_ticks;
    }

    void add(const Counters& src) {
        const T* s = reinterpret_cast<const T*>(&src);
        const T* e = s + sizeof(Counters) / sizeof(T);
        static_assert(sizeof(Counters) % sizeof(T) == 0);
        T* d = reinterpret_cast<T*>(this);
        while (s != e) *d++ += *s++;
    }
};
