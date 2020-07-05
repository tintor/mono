#pragma once
#include "geom/edges.h"
#include "geom/segment.h"

template <typename Vec>
struct triangle {
    Vec a, b, c;

    triangle() {}
    constexpr triangle(Vec a, Vec b, Vec c) : a(a), b(b), c(c) {}
    constexpr triangle(const triangle& v) : a(v.a), b(v.b), c(v.c) {}

    Vec& operator[](int idx) { return (&a)[idx]; }
    Vec operator[](int idx) const { return (&a)[idx]; }

    std::array<segment<Vec>, 3> edges() const { return {segment{a, b}, segment{b, c}, segment{c, a}}; }

    bool operator==(triangle v) const { return equal(a, v.a) && equal(b, v.b) && equal(c, v.c); }
    bool operator!=(triangle v) const { return !operator==(v); }

    auto begin() { return &a; }
    auto end() { return &a + 3; }

    using const_iterator = const Vec*;
    auto begin() const { return &a; }
    auto end() const { return &a + 3; }
};

template <typename T>
constexpr auto Edges(const triangle<T>& m) {
    return m.edges();
}

using triangle2 = triangle<double2>;
using triangle3 = triangle<double3>;

/*template <typename Vec>
void format_e(string& s, string_view spec, triangle<Vec> v) {
    format_s(s, "(%s, %s, %s)", v.a, v.b, v.c);
}*/

inline double3 compute_normal(triangle3 v) { return compute_normal(v.a, v.b, v.c); }

inline double signed_double_area(triangle2 m) { return signed_double_area(m.a, m.b, m.c); }

inline double area(double2 a, double2 b, double2 c) { return abs(signed_double_area(a, b, c)) / 2; }

inline double area(triangle2 m) { return abs(signed_double_area(m)) / 2; }
