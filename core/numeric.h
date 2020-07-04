#pragma once
#include <limits>
#include <cmath>
#include "core/auto.h"

constexpr double PI = M_PI;
constexpr double INF = std::numeric_limits<double>::infinity();

// --------

static_assert(sizeof(char) == 1);
static_assert(sizeof(short) == 2);
static_assert(sizeof(int) == 4);
static_assert(sizeof(long) == 8);
static_assert(sizeof(long long) == 8);

using uchar = unsigned char;

using uint = unsigned int;
using ulong = unsigned long;

using cent = __int128;
using ucent = __uint128_t;

using size_t = ulong;

// --------

template <typename Int>
inline bool add_overflow(Int a, Int b) {
    if (b >= 0) return std::numeric_limits<Int>::max() - b < a;
    return a < std::numeric_limits<Int>::min() - b;
}

// --------

#define FOR_INTERNAL(I, COUNT, VAR) for (std::remove_const_t<std::remove_reference_t<decltype(COUNT)>> VAR = COUNT, I = 0; I < VAR; I++)
#define FOR(I, COUNT) FOR_INTERNAL(I, COUNT, TOKEN_PASTE(_count_, __COUNTER__))

// --------

#define FORMAT_VEC(T, N) using T##N = T __attribute__((ext_vector_type(N)));
#define FORMAT_VECN(T) FORMAT_VEC(T, 2); FORMAT_VEC(T, 3); FORMAT_VEC(T, 4); FORMAT_VEC(T, 8)

FORMAT_VECN(int);
FORMAT_VECN(long);
FORMAT_VECN(cent);
FORMAT_VECN(float);
FORMAT_VECN(double);

#undef FORMAT_VECN
#undef FORMAT_VEC
