#pragma once
#include "core/auto.h"
#include "core/numeric.h"

struct Timestamp {
    Timestamp() : _ticks(__builtin_readcyclecounter()) {}
    Timestamp(ulong a) : _ticks(a) {}

    ulong elapsed(Timestamp a = Timestamp()) const { return a._ticks - _ticks; }

    double elapsed_s(Timestamp a = Timestamp()) const { return elapsed(a) * _ms_per_tick * 1e-3; }
    double elapsed_ms(Timestamp a = Timestamp()) const { return elapsed(a) * _ms_per_tick; }

    static void init();

    static double to_s(ulong ticks) { return ticks * _ms_per_tick * 1e-3; }
    static double ms_per_tick() { return _ms_per_tick; }

   private:
    ulong _ticks;
    static double _ms_per_tick;
};

template <typename Func>
ulong Duration(const Func& func) {
    Timestamp start;
    func();
    return start.elapsed();
}

/*template<typename Func>
auto measure_internal(string_view name, Func func) {
        Timestamp ts;
        ON_SCOPE_EXIT(print("%s took %s ms\n", name, ts.elapsed_ms()));
        return func();
}

#define MEASURE(X) measure(#X, [&](){ return X; } )*/

template <typename Func, typename Ticks>
auto timer_internal(const Func& func, Ticks& ticks) {
    Timestamp ts;
    ON_SCOPE_EXIT(ticks += ts.elapsed());
    return func();
}

#define TIMER(EXPR, TICKS) timer_internal([&]() { return (EXPR); }, TICKS)
