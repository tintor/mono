#pragma once
#include "core/numeric.h"
#include "core/align_alloc.h"
#include <immintrin.h>
#include <random>

#define vshuffle __builtin_shufflevector
#define vconvert __builtin_convertvector

template<typename T, int N>
using vec = T __attribute__((ext_vector_type(N)));

template<typename T>
using vec2 = vec<T, 2>;
template<typename T>
using vec3 = vec<T, 3>;
template<typename T>
using vec4 = vec<T, 4>;
template<typename T>
using vec8 = vec<T, 8>;

namespace std {
template <typename T, int N>
class allocator<vec<T, N>> : public AlignAlloc<vec<T, N>> {};
}  // namespace std

template<typename T>
std::ostream& operator<<(std::ostream& os, vec2<T> a) {
    return os << a.x << ' ' << a.y;
}

template<typename T>
std::ostream& operator<<(std::ostream& os, vec3<T> a) {
    return os << a.x << ' ' << a.y << ' ' << a.z;
}

template<typename T>
std::ostream& operator<<(std::ostream& os, vec4<T> a) {
    return os << a.x << ' ' << a.y << ' ' << a.z << ' ' << a.w;
}

template<typename T>
vec4<T> cast4(vec3<T> a) { return vshuffle(a, a, 0, 1, 2, -1); }

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

#define REQUIRE_NEAR(A, B, T)                                                             \
    do {                                                                                  \
        auto aa = A;                                                                      \
        auto bb = B;                                                                      \
        auto tt = T;                                                                      \
        auto dd = length(aa - bb);                                                        \
        REQUIRE(dd <= tt, "a = %s\nb = %s\nexpected <= %s, actual = %s", aa, bb, tt, dd); \
    } while (false);

#define REQUIRE_EQUAL(A, B)                                                               \
    do {                                                                                  \
        auto aa = A;                                                                      \
        auto bb = B;                                                                      \
        REQUIRE(all(aa == bb), "a = %s\nb = %s\n|a-b| = %g", aa, bb, abs(aa - bb)); \
    } while (false);

inline int2 i2(int x, int y) { return {x, y}; }
inline int3 i3(int x, int y, int z) { return {x, y, z}; }
inline int4 i4(int x, int y, int z, int w) { return {x, y, z, w}; }

inline float2 f2(float x, float y) { return {x, y}; }
inline float3 f3(float x, float y, float z) { return {x, y, z}; }
inline float4 f4(float x, float y, float z, float w) { return {x, y, z, w}; }

inline double2 d2(double x, double y) { return {x, y}; }
inline double3 d3(double x, double y, double z) { return {x, y, z}; }
inline double4 d4(double x, double y, double z, double w) { return {x, y, z, w}; }

template<typename T>
inline vec3<T> extend(vec2<T> v, T z) { return {v.x, v.y, z}; }
template<typename T>
inline vec4<T> extend(vec3<T> v, T w) { return {v.x, v.y, v.z, w}; }

template<typename T>
inline vec<T, 2> broad2(T a) { return {a, a}; }
template<typename T>
inline vec<T, 3> broad3(T a) { return {a, a, a}; }
template<typename T>
inline vec<T, 4> broad4(T a) { return {a, a, a, a}; }

template<typename T>
inline void broadcast(vec2<T>& a, T b) { a = {b, b}; }
template<typename T>
inline void broadcast(vec3<T>& a, T b) { a = {b, b, b}; }
template<typename T>
inline void broadcast(vec4<T>& a, T b) { a = {b, b, b, b}; }

template<typename T>
inline bool lex_less(vec2<T> a, vec2<T> b) {
    if (a.x < b.x) return true;
    if (a.x > b.x) return false;
    return a.y < b.y;
}

template<typename T>
inline bool lex_less(vec3<T> a, vec3<T> b) {
    if (a.x < b.x) return true;
    if (a.x > b.x) return false;
    if (a.y < b.y) return true;
    if (a.y > b.y) return false;
    return a.z < b.z;
}

template<typename T>
inline bool lex_less(vec4<T> a, vec4<T> b) {
    if (a.x < b.x) return true;
    if (a.x > b.x) return false;
    if (a.y < b.y) return true;
    if (a.y > b.y) return false;
    if (a.z < b.z) return true;
    if (a.z > b.z) return false;
    return a.w < b.w;
}

inline float2 sqrt(float2 a) { return f2(sqrt(a.x), sqrt(a.y)); }
inline float3 sqrt(float3 a) { return float4(_mm_sqrt_ps(cast4(a))).xyz; }
inline float4 sqrt(float4 a) { return _mm_sqrt_ps(a); }

inline double2 sqrt(double2 a) { return _mm_sqrt_pd(a); }
inline double3 sqrt(double3 a) { return double4(_mm256_sqrt_pd(cast4(a))).xyz; }
inline double4 sqrt(double4 a) { return _mm256_sqrt_pd(a); }

#define m128 __m128d
#define m256 __m256d
#define m512 __m512d

// use m128 and m256 to allow input of differrent types (but equal size)
inline m128 bit_and(m128 a, m128 b) { return _mm_and_pd(a, b); }
inline m256 bit_and(m256 a, m256 b) { return _mm256_and_pd(a, b); }

inline m128 bit_or(m128 a, m128 b) { return _mm_or_pd(a, b); }
inline m256 bit_or(m256 a, m256 b) { return _mm256_or_pd(a, b); }

inline m128 bit_xor(m128 a, m128 b) { return _mm_xor_pd(a, b); }
inline m256 bit_xor(m256 a, m256 b) { return _mm256_xor_pd(a, b); }

// returns 1.0 or -1.0 for each component (depending if >= +0, or <= -0)
inline double4 sign_no_zero(double4 d) { return bit_and(bit_or(d, broad4(1.0)), broad4(-1.0)); }
inline double3 sign_no_zero(double3 d) { return sign_no_zero(cast4(d)).xyz; }

inline float4 vdot(float4 a, float4 b) { return _mm_dp_ps(a, b, 0b11111111); }

inline double2 vdot(double2 a, double2 b) { return _mm_dp_pd(a, b, 0b11111111); }
// 4 instructions vs 1 instruction for float4
inline double4 vdot(double4 a, double4 b) {
    // There is no _mm256_dp_pd! But there is _mm_dp_ps for floats!
    // return _mm256_dp_pd(a, a, 0b11111111);
    double4 ab = a * b;
    double4 q = _mm256_hadd_pd(ab, ab);
    return q + q.zwxy;
}

inline float dot(float4 a, float4 b) { return vdot(a, b).x; }

inline double dot(double2 a, double2 b) { return vdot(a, b).x; }
inline double dot(double3 a, double3 b) {
    double3 q = a * b;
    return q.x + q.y + q.z;
}
inline double dot(double4 a, double4 b) { return vdot(a, b).x; }

inline constexpr double squared(double a) { return a * a; }
inline double squared(double2 a) { return dot(a, a); }
inline double squared(double3 a) { return dot(a, a); }
inline double squared(double4 a) { return dot(a, a); }

inline double2 vlength(double2 a) { return sqrt(vdot(a, a)); }
inline double4 vlength(double4 a) { return sqrt(vdot(a, a)); }

inline constexpr double length(double a) { return (a >= 0) ? a : -a; }
inline double length(double2 a) { return vlength(a).x; }
inline double length(double3 a) { return sqrt(dot(a, a)); }
inline double length(double4 a) { return vlength(a).x; }

// TODO div(sqrt) might be slower than mul(rsqrt)
inline auto normalize(double2 a) { return a / vlength(a); }
inline auto normalize(double3 a) { return a / length(a); }
inline auto normalize(double4 a) { return a / vlength(a); }

template <typename V>
bool is_unit(V v) {
    return ordered(1 - 1e-12, squared(v), 1 + 1e-12);
}

template <int M>
double2 round(double2 v, int mode) {
    return _mm_round_pd(v, M | _MM_FROUND_NO_EXC);
}
template <int M>
double3 round(double3 v, int mode) {
    double4 e = _mm256_round_pd(cast4(v), M | _MM_FROUND_NO_EXC);
    return e.xyz;
}
template <int M>
double4 round(double4 v, int mode) {
    return _mm256_round_pd(v, M | _MM_FROUND_NO_EXC);
}

template <typename V>
V round_nearest(V v) {
    return round<_MM_FROUND_TO_NEAREST_INT>(v);
}
template <typename V>
V round_zero(V v) {
    return round<_MM_FROUND_TO_ZERO>(v);
}
template <typename V>
V round_up(V v) {
    return round<_MM_FROUND_TO_POS_INF>(v);
}
template <typename V>
V round_down(V v) {
    return round<_MM_FROUND_TO_NEG_INF>(v);
}

constexpr ulong SignBit64 = ulong(1) << 63;
inline double2 abs(double2 v) { return bit_and(v, broad2(~SignBit64)); }
inline double3 abs(double3 v) { return double4(bit_and(cast4(v), broad4(~SignBit64))).xyz; }
inline double4 abs(double4 v) { return bit_and(v, broad4(~SignBit64)); }

inline double3 any_normal(double3 v) {
    double3 a = abs(v);
    if (a.x <= a.y && a.x <= a.z) return {0, -v.z, v.y};
    if (a.y <= a.z) return {-v.z, 0, v.x};
    return {-v.y, v.x, 0};
}

template<typename T>
inline T cross(vec2<T> a, vec2<T> b) { return a.x * b.y - b.x * a.y; }

template<typename T>
inline vec3<T> cross(vec3<T> a, vec3<T> b) {
    return {a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y};
}

// returns angle in range [0, PI]
inline auto angle(double3 a, double3 b) { return std::atan2(length(cross(a, b)), dot(a, b)); }

// solid angle between triangle and origin
inline double solid_angle(double3 A, double3 B, double3 C) {
    double y = dot(A, cross(B, C));
    double a = length(A), b = length(B), c = length(C);
    double x = a * b * c + c * dot(A, B) + b * dot(A, C) + a * dot(B, C);
    return 2 * std::atan2(y, x);
}

// det(A) == det(transposed(A))
inline double det(double2 a, double2 b) { return a.x * b.y - b.x * a.y; }

// det(A) == det(transposed(A))
// 3x3 det
inline double det(double3 a, double3 b, double3 c) {
    return a.x * det(b.yz, c.yz) - b.x * det(a.yz, c.yz) + c.x * det(a.yz, b.yz);
}

// det(A) == det(transposed(A))
inline double det(double4 a, double4 b, double4 c, double4 d) {
    return a.x * det(b.yzw, c.yzw, d.yzw) - b.x * det(a.xzw, c.xzw, d.xzw) + c.x * det(a.xyw, b.xyw, d.xyw) -
           d.x * det(a.xyz, b.xyz, c.xyz);
}

template <typename RND>
double uniform(RND& rnd, double min, double max) {
    return std::uniform_real_distribution<double>(min, max)(rnd);
}

// uniform inside a cube
template <typename RND>
double2 uniform2(RND& rnd, double min, double max) {
    return {uniform(rnd, min, max), uniform(rnd, min, max)};
}

// uniform point inside a cube
template <typename RND>
double3 uniform3(RND& rnd, double min, double max) {
    return {uniform(rnd, min, max), uniform(rnd, min, max), uniform(rnd, min, max)};
}

template <typename RND>
double4 uniform4(RND& rnd, double min, double max) {
    return {uniform(rnd, min, max), uniform(rnd, min, max), uniform(rnd, min, max), uniform(rnd, min, max)};
}

template <typename RND>
inline double normal(RND& rnd, double mean, double stdev) {
    return std::normal_distribution<double>(mean, stdev)(rnd);
}

template <typename RND>
double2 normal2(RND& rnd, double mean, double stdev) {
    return {normal(rnd, mean, stdev), normal(rnd, mean, stdev)};
}

template <typename RND>
double3 normal3(RND& rnd, double mean, double stdev) {
    return {normal(rnd, mean, stdev), normal(rnd, mean, stdev), normal(rnd, mean, stdev)};
}

template <typename RND>
double4 normal4(RND& rnd, double mean, double stdev) {
    return {normal(rnd, mean, stdev), normal(rnd, mean, stdev), normal(rnd, mean, stdev), normal(rnd, mean, stdev)};
}

template <typename RND>
double2 uniform_dir2(RND& rnd) {
    double a = uniform(rnd, 0, 2 * PI);
    return {cos(a), sin(a)};
}

template <typename RND>
double3 uniform_dir3(RND& rnd) {
    return normalize(normal3(rnd, 0, 1));
}

template <typename RND>
double4 uniform_dir4(RND& rnd) {
    return normalize(normal4(rnd, 0, 1));
}

inline bool colinear(double3 a, double3 b, double3 c) { return squared(cross(b - a, c - a)) <= squared(1e-6); }

inline double3 compute_normal(double3 a, double3 b, double3 c) { return cross(b - a, c - a); }

// Finds E such that:
// x[i] * e.x + y[i] * e.y + z[i] * e.z = w[i]
inline double3 solve_linear_col(double3 x, double3 y, double3 z, double3 w) {
    double3 e = {det(w, y, z), det(x, w, z), det(x, y, w)};
    return e / det(x, y, z);
}

// Finds E such that:
// dot(a, e) = w[0]
// dot(b, e) = w[1]
// dot(c, e) = w[2]
inline double3 solve_linear_row(double3 a, double3 b, double3 c, double3 w) {
    double3 x = {a.x, b.x, c.x};
    double3 y = {a.y, b.y, c.y};
    double3 z = {a.z, b.z, c.z};
    return solve_linear_col(x, y, z, w);
}

constexpr double Tolerance = 0.5e-6;

// TODO Two different equals for vectors?
template <typename T, int N>
bool Equals(vec<T, N> a, vec<T, N> b) {
    return squared(a - b) <= squared(Tolerance);
}
