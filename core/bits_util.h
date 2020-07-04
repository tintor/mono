#pragma once
#include "core/numeric.h"

// Note: undefined if x == 0
// TESTED
inline int clz(uint x) {
    static_assert(sizeof(unsigned int) == 4);
    return __builtin_clz(x);
}

inline int clz(ulong x) {
    static_assert(sizeof(unsigned long) == 8);
    return __builtin_clzl(x);
}

inline int clz(ucent x) {
    ulong a = x >> 64;
    ulong b = x;
    return a == 0 ? 64 + clz(b) : clz(a);
}

inline int ctz(uint x) {
    static_assert(sizeof(unsigned int) == 4);
    return __builtin_ctz(x);
}

inline int ctz(ulong x) {
    static_assert(sizeof(unsigned long) == 8);
    return __builtin_ctzl(x);
}

// TESTED
inline int popcount(uint x) {
    static_assert(sizeof(unsigned int) == 4);
    return __builtin_popcount(x);
}
inline int popcount(ulong x) {
    static_assert(sizeof(unsigned long) == 8);
    return __builtin_popcountl(x);
}
inline int popcount(__uint128_t x) {
    ulong a = x >> 64;
    ulong b = x;
    return popcount(a) + popcount(b);
}

inline uint is_power2(uint a) { return (a & (a - 1)) == 0; }

inline ulong is_power2(ulong a) { return (a & (a - 1)) == 0; }

inline ucent is_power2(ucent a) { return (a & (a - 1)) == 0; }

// TESTED
inline uint round_up_power2(uint a) {
    a -= 1;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a += 1;
    return a;
}

inline ulong round_up_power2(ulong a) {
    a -= 1;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a |= a >> 32;
    a += 1;
    return a;
}

inline ucent round_up_power2(ucent a) {
    a -= 1;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a |= a >> 32;
    a |= a >> 64;
    a += 1;
    return a;
}
