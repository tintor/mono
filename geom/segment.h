#pragma once
#include <cassert>

#include "geom/vector.h"

constexpr double Tolerance = 0.5e-6;

template <typename Vec>
bool Equals(Vec a, Vec b) {
    return squared(a - b) <= squared(Tolerance);
}

// finite part of line between two points
template <typename Vec>
struct segment {
    Vec a, b;

    constexpr segment(Vec a, Vec b) : a(a), b(b) {}
    constexpr segment(const segment& s) : a(s.a), b(s.b) {}

    constexpr bool operator==(segment v) const { return equal(a, v.a) && equal(b, v.b); }
    constexpr bool operator!=(segment v) const { return !(operator==(v)); }

    segment reversed() const { return {b, a}; }

    Vec linear(double t) const { return a + (b - a) * t; }

    // Inverse of linear
    double param(Vec p) const {
        Vec d = b - a;
        return dot(p - a, d) / dot(d, d);
    }

    // Return point on segment that is nearest to P
    Vec nearest(Vec p) const {
        Vec d = b - a;
        double t = dot(p - a, d);
        if (t <= 0) return a;
        if (t >= dot(d, d)) return b;
        return a + d * (t / dot(d, d));
    }
};

using segment2 = segment<double2>;
using segment3 = segment<double3>;

// starts from a, and goes to infinity
template <typename Vec>
struct ray {
    const Vec unit_dir, origin;

    constexpr ray(const ray& s) : unit_dir(s.unit_dir), origin(s.origin) {}
    explicit ray(segment<Vec> s) : unit_dir(normalize(s.b - s.a)), origin(s.a) {}
    ray(Vec a, Vec b) : unit_dir(normalize(b - a)), origin(a) {}

    // using max() instead of infinity() in order for mult with zero to stay zero
    Vec infinity() const { return linear(std::numeric_limits<double>::max()) * std::numeric_limits<double>::max(); }

    bool operator==(ray v) const { return equal(origin, v.origin) && equal(unit_dir, v.unit_dir); }
    bool operator!=(ray v) const { return !(operator==(v)); }

    Vec linear(double t) const { return origin + unit_dir * t; }

    // Inverse of linear
    double param(Vec p) const { return dot(p - origin, unit_dir); }
};

using ray2 = ray<double2>;
using ray3 = ray<double3>;

// TODO raw_line3: which just stores points A and B (no normalization)

// infinite on both sides unlike segment
// Note: dir may be flipped after construction!
template <typename Vec>
struct line {
    const Vec unit_dir, origin;

    static Vec normalize_dir(Vec d) {
        if (d.x < 0) return -d;
        if (d.x == 0 && d.y < 0) return -d;
        if (d.x == 0 && d.y == 0 && d.z < 0) return -d;
        return d;
    }

    line(const line& s) : unit_dir(s.unit_dir), origin(s.origin) {}
    explicit line(ray<Vec> s)
        : unit_dir(normalize_dir(s.unit_dir)), origin(s.origin - unit_dir * dot(s.origin, unit_dir)) {}
    explicit line(segment<Vec> s) : line(ray(s)) {}
    line(Vec a, Vec b) : line(segment<Vec>{a, b}) {}

    // TODO(marko) equality condition should check if lines are overlapping
    bool operator==(line<Vec> v) const { return equal(origin, v.origin) && equal(unit_dir, v.unit_dir); }
    bool operator!=(line<Vec> v) const { return !(operator==(v)); }

    Vec linear(double t) const { return origin + unit_dir * t; }

    Vec nearest(Vec p) const { return origin + unit_dir * dot(p - origin, unit_dir); }
};

using line2 = line<double2>;
using line3 = line<double3>;

bool relate(segment<double2> p, ray<double2> q);

// Tested and stable!
enum class NearestCase {
    RayRay = 0,
    RayPoint = 1,
    PointPoint = 2,
};

std::pair<segment3, NearestCase> nearest(segment3 p, segment3 q);

/*template <typename Vec>
void format_e(string& s, string_view spec, line<Vec> v) {
    format_s(s, "line(%s, %s)", v.origin, v.unit_dir);
}

template <typename Vec>
void format_e(string& s, string_view spec, ray<Vec> v) {
    format_s(s, "ray(%s, %s)", v.origin, v.unit_dir);
}

template <typename Vec>
void format_e(string& s, string_view spec, segment<Vec> v) {
    format_s(s, "segment(%s, %s)", v.a, v.b);
}

template <typename Vec>
hash operator<<(hash h, line<Vec> s) {
    return h << s.origin << s.unit_dir;
}
template <typename Vec>
hash operator<<(hash h, ray<Vec> s) {
    return h << s.origin << s.unit_dir;
}
template <typename Vec>
hash operator<<(hash h, segment<Vec> s) {
    return h << s.a << s.b;
}*/

inline auto angle(segment3 p, segment3 q) { return angle(p.b - p.a, q.b - q.a); }

inline double signed_double_edge_area(double2 a, double2 b) { return (a.x + b.x) * (a.y - b.y); }

inline double signed_double_area(double2 a, double2 b, double2 c) {
    return signed_double_edge_area(a, b) + signed_double_edge_area(b, c) + signed_double_edge_area(c, a);
}

inline bool Colinear(line3 s, double3 v) {
    auto va = v - s.origin;
    return squared(va - s.unit_dir * dot(va, s.unit_dir)) <= squared(Tolerance);
}

bool relate_abxo(segment2 p, segment2 q);
bool relate_abxo(segment2 p, segment2 q, double inv_p_len);

char relate(segment2 p, segment2 q, double2* pt = nullptr, double2* qt = nullptr);

constexpr int COLINEAR = 1;
constexpr int VERTEX_VERTEX = 2;
constexpr int VERTEX_EDGE = 4;
constexpr int EDGE_VERTEX = 8;
constexpr int CROSS = 16;
constexpr int OVERLAP = 32;
constexpr int PA = 64;
constexpr int PB = 128;
constexpr int QA = 256;
constexpr int QB = 512;
int relate_full(segment2 p, segment2 q, double* pt = nullptr, double* qt = nullptr);
