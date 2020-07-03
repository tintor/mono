#pragma once
#include <limits>

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

template <typename Int>
inline bool add_overflow(Int a, Int b) {
    if (b >= 0) return std::numeric_limits<Int>::max() - b < a;
    return a < std::numeric_limits<Int>::min() - b;
}
