#pragma once
#include "core/numeric.h"
#include <immintrin.h>

#define vshuffle __builtin_shufflevector
#define vconvert __builtin_convertvector

template<typename T, int N>
using vec = T __attribute__((ext_vector_type(N)));

template<typename T>
vec<T, 4> cast4(vec<T, 3> a) { return vshuffle(a, a, 0, 1, 2, -1); }

inline bool all(int2 v) { return v.x != 0 && v.y != 0; }
inline bool all(int3 v) { return _mm_movemask_ps(vshuffle(v, v, 0, 1, 2, 0)) == 0xF; }
inline bool all(int4 v) { return _mm_movemask_ps(v) == 0xF; }
inline bool all(int8 v) { return _mm256_movemask_ps(v) == 0xFF; }

inline bool all(long2 v) { return _mm_movemask_pd(v) == 0x3; }
inline bool all(long3 v) { return _mm256_movemask_pd(vshuffle(v, v, 0, 1, 2, 0)) == 0xF; }
inline bool all(long4 v) { return _mm256_movemask_pd(v) == 0xF; }

inline bool any(int2 v) { return v.x != 0 || v.y != 0; }
inline bool any(int3 v) { return _mm_movemask_ps(vshuffle(v, v, 0, 1, 2, 0)) != 0; }
inline bool any(int4 v) { return _mm_movemask_ps(v) != 0; }
inline bool any(int8 v) { return _mm256_movemask_ps(v) != 0; }

inline bool any(long2 v) { return _mm_movemask_pd(v) != 0; }
inline bool any(long3 v) { return _mm256_movemask_pd(vshuffle(v, v, 0, 1, 2, 0)) != 0; }
inline bool any(long4 v) { return _mm256_movemask_pd(v) != 0; }

template<typename T, int N>
bool equal(vec<T, N> a, vec<T, N> b) { return all(a == b); }

inline double2 floor(double2 a) { return _mm_floor_pd(a); }
inline double3 floor(double3 a) { return double4(_mm256_floor_pd(cast4(a))).xyz; }
inline double4 floor(double4 a) { return _mm256_floor_pd(a); }

inline float2 floor(float2 a) { return float2{std::floor(a.x), std::floor(a.y)}; }
inline float3 floor(float3 a) { return float4(_mm_floor_ps(cast4(a))).xyz; }
inline float4 floor(float4 a) { return _mm_floor_ps(a); }
inline float8 floor(float8 a) { return _mm256_floor_ps(a); }

inline double2 ceil(double2 a) { return _mm_ceil_pd(a); }
inline double3 ceil(double3 a) { return double4(_mm256_ceil_pd(cast4(a))).xyz; }
inline double4 ceil(double4 a) { return _mm256_ceil_pd(a); }

inline float2 ceil(float2 a) { return float2{std::ceil(a.x), std::ceil(a.y)}; }
inline float3 ceil(float3 a) { return float4(_mm_ceil_ps(cast4(a))).xyz; }
inline float4 ceil(float4 a) { return _mm_ceil_ps(a); }
inline float8 ceil(float8 a) { return _mm256_ceil_ps(a); }

template <typename T>
struct equal_t {
    bool operator()(T a, T b) const { return equal(a, b); }
};

inline int8 vmin(int8 a, int8 b) { return _mm256_min_epi32(a, b); }
inline int4 vmin(int4 a, int4 b) { return _mm_min_epi32(a, b); }

inline float8 vmin(float8 a, float8 b) { return _mm256_min_ps(a, b); }
inline float4 vmin(float4 a, float4 b) { return _mm_min_ps(a, b); }

inline double2 vmin(double2 a, double2 b) { return _mm_min_pd(a, b); }
inline double3 vmin(double3 a, double3 b) { return double4(_mm256_min_pd(cast4(a), cast4(b))).xyz; }
inline double4 vmin(double4 a, double4 b) { return _mm256_min_pd(a, b); }

inline int8 vmax(int8 a, int8 b) { return _mm256_max_epi32(a, b); }
inline int4 vmax(int4 a, int4 b) { return _mm_max_epi32(a, b); }

inline float8 vmax(float8 a, float8 b) { return _mm256_max_ps(a, b); }
inline float4 vmax(float4 a, float4 b) { return _mm_max_ps(a, b); }

inline double2 vmax(double2 a, double2 b) { return _mm_max_pd(a, b); }
inline double3 vmax(double3 a, double3 b) { return double4(_mm256_max_pd(cast4(a), cast4(b))).xyz; }
inline double4 vmax(double4 a, double4 b) { return _mm256_max_pd(a, b); }

inline int hmin(int8 a) {
    auto b = vmin(a, vshuffle(a, a, 2, 3, -1, -1, 6, 7, -1, -1));
    auto c = vmin(b, vshuffle(b, b, 1, -1, -1, -1, 5, -1, -1, -1));
    auto d = vmin(c, vshuffle(c, c, 4, -1, -1, -1, -1, -1, -1, -1));
    return d.x;
}

inline float hmin(float8 a) {
    auto b = vmin(a, vshuffle(a, a, 2, 3, -1, -1, 6, 7, -1, -1));
    auto c = vmin(b, vshuffle(b, b, 1, -1, -1, -1, 5, -1, -1, -1));
    auto d = vmin(c, vshuffle(c, c, 4, -1, -1, -1, -1, -1, -1, -1));
    return d.x;
}

inline int hmax(int8 a) {
    auto b = vmax(a, vshuffle(a, a, 2, 3, -1, -1, 6, 7, -1, -1));
    auto c = vmax(b, vshuffle(b, b, 1, -1, -1, -1, 5, -1, -1, -1));
    auto d = vmax(c, vshuffle(c, c, 4, -1, -1, -1, -1, -1, -1, -1));
    return d.x;
}

inline float hmax(float8 a) {
    auto b = vmax(a, vshuffle(a, a, 2, 3, -1, -1, 6, 7, -1, -1));
    auto c = vmax(b, vshuffle(b, b, 1, -1, -1, -1, 5, -1, -1, -1));
    auto d = vmax(c, vshuffle(c, c, 4, -1, -1, -1, -1, -1, -1, -1));
    return d.x;
}
