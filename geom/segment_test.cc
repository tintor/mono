#include "geom/segment.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("segment", "[segment]") {
}

#include "core/vector.h"
#include "core/check.h"
#include "core/string.h"
#include <fmt/format.h>
using Point2d = double2;
using Polygon2d = std::vector<Point2d>;

double cross(segment2 p, double2 q) {
    return cross(p.b - p.a, q);
}

bool is_inside_of(double2 p, segment2 q) {
    //return cross(q.b - q.a, p - q.a) >= 0;
    return cross(q, p - q.a) >= 0;
}

double cross(segment2 p, segment2 q) {
    return cross(p.b - p.a, q.b - q.a);
}

bool aim(segment2 p, segment2 q) {
    bool inside = is_inside_of(p.b, q);
    double c = cross(q, p);
    return (inside && c < 0) || (!inside && c >= 0);
}

bool contains(const Polygon2d& poly, double2 a) {
    for (size_t j = poly.size() - 1, i = 0; i < poly.size(); j = i++)
        if (!is_inside_of(a, {poly[j], poly[i]})) return false;
    return true;
}

#ifdef XXX
Polygon2d ConvexIntersection(const Polygon2d& poly_a, const Polygon2d& poly_b) {
  //fmt::print("---\n");
  Polygon2d result;

  char inside = ' ';
  uint fa = -1, fb = -1; // two edges with the first intersection
  uint first_intersection_iteration = -1;
  uint ia = 0, ib = 0;

  double2 a = poly_a[ia], b = poly_b[ib];
  segment2 ea(poly_a.back(), a);
  segment2 eb(poly_b.back(), b);

  const uint max_iterations = 2 * (poly_a.size() + poly_b.size());
  for (uint iteration = 0; iteration < max_iterations; iteration++) {
    //fmt::print("ia:{} ib:{}\n", ia, ib);

    // Edge intersection check (parallel case is ignored).
    double2 common;
    bool intersecting = false;
    if (true) {
        double2 t;
        char r = relate(ea, eb, &t, nullptr);
        intersecting = r == 'A' || r == 'B' || r == 'X' || r == 'V';
        if (intersecting) common = ea.linear(t.x);
    } else {
        intersecting = relate_non_parallel(ea, eb, &common);

        double2 common2;
        double2 t;
        char r = relate(ea, eb, &t, nullptr);
        bool intersecting2 = r == 'A' || r == 'B' || r == 'X' || r == 'V';
        if (intersecting2) common2 = ea.linear(t.x);

        /*if (intersecting != intersecting2) {
            fmt::print("ea.a {} {}\n", double(ea.a.x), double(ea.a.y));
            fmt::print("ea.b {} {}\n", double(ea.b.x), double(ea.b.y));
            fmt::print("eb.a {} {}\n", double(eb.a.x), double(eb.a.y));
            fmt::print("eb.b {} {}\n", double(eb.b.x), double(eb.b.y));
            fmt::print("{} {}\n", intersecting, intersecting2);
            Check(false);
        }
        if (intersecting) Check(equal(common, common2));*/
    }

    if (intersecting) {
        //if (ia == fa && ib == fb) return result;

        //fmt::print("common {} {}\n", double(common.x), double(common.y));
        if (fa == uint(-1)) {
            fa = ia;
            fb = ib;
        }

        if (result.empty() || iteration != first_intersection_iteration + 1)
            if (result.size() > 0 && equal(result[0], common)) return result;

        if (first_intersection_iteration == iteration)
            first_intersection_iteration = iteration;

        if (result.empty() || !equal(result.back(), common)) {
            //if (result.size() > 0 && equal(result[0], common)) return result;
            //fmt::print("common add {} {}\n", double(common.x), double(common.y));
            result.push_back(common);
        }
        inside = is_inside_of(a, eb) ? 'A' : 'B';
        //fmt::print("inside {}\n", inside);
    }
    // Ignoring overlap AND parallel cases for now.
    /*double2 R = ea.a - eb.a, Q = eb.b - eb.a, P = ea.b - ea.a;
    double d = cross(Q, P);
    double s = cross(R, Q) / d;
    double t = cross(R, P) / d;*/

    // Advance either ia or ib.
    bool advance_a = false;
    bool advance_b = false;
    bool advance_outside = false;

    double c = cross(eb, ea);
    /*if ((c >= 0 && !is_inside_of(a, eb)) || (c < 0 && is_inside_of(b, ea))) {
        // Advance ia.
        if (inside == 'A' && !equal(result.back(), a) && !equal(result[0], a)) {
            //fmt::print("add {} {}\n", double(a.x), double(a.y));
            result.push_back(a);
        }
        if (++ia == poly_a.size()) ia = 0;
        a = poly_a[ia];
        ea = segment2(ea.b, a);
    } else {
        // Advance ib.
        if (inside == 'B' && !equal(result.back(), b) && !equal(result[0], b)) {
            //fmt::print("add {} {}\n", double(b.x), double(b.y));
            result.push_back(b);
        }
        if (++ib == poly_b.size()) ib = 0;
        b = poly_b[ib];
        eb = segment2(eb.b, b);
    }*/
    if (aim(eb, ea)) {
        if (aim(ea, eb)) {
            advance_outside = true;
        } else {
            advance_b = true;
        }
    } else {
        if (aim(ea, eb)) {
            advance_a = true;
        } else {
            advance_outside = true;
        }
    }

    if (advance_outside) {
        if (!is_inside_of(a, eb)) advance_a = true;
        else if (!is_inside_of(b, ea)) advance_b = true;
        //Check(advance_a != advance_b);
    }

    if (advance_a) {
        // Advance ia.
        if (inside == 'A' && !equal(result.back(), a) && !equal(result[0], a)) {
            //fmt::print("add {} {}\n", double(a.x), double(a.y));
            result.push_back(a);
        }
        if (++ia == poly_a.size()) ia = 0;
        a = poly_a[ia];
        ea = segment2(ea.b, a);
    }
    if (advance_b) {
        // Advance ib.
        if (inside == 'B' && !equal(result.back(), b) && !equal(result[0], b)) {
            //fmt::print("add {} {}\n", double(b.x), double(b.y));
            result.push_back(b);
        }
        if (++ib == poly_b.size()) ib = 0;
        b = poly_b[ib];
        eb = segment2(eb.b, b);
    }
  }
  //fmt::print("boundaries not intersecting\n");

  // Boundaries are not intersecting.
  if (contains(poly_b, poly_a[0])) return poly_b;
  if (contains(poly_a, poly_b[0])) return poly_a;
  return {};
}

std::string ToString(double2 a) {
    return fmt::format("{} {}", double(a.x), double(a.y));
}
#endif

// Complexity: O(|A| + |B|)
// Assumes that both input polygons are convex and counter-clockwise.
// Degenerate cases are ignored (boundaries can only cleanly intersect)!
// Returns true if polygon boundaries are intersecting.
// If polygon boundaries are intersecting, sends boundary of intersection to result visitor.
// Based on: O'Rourke et al. 1981 "A New Linear Algorithm for Intersecting Convex Polygons"
template<bool EarlyExit, typename Result>
bool ConvexIntersectionGeneric(const Polygon2d& poly_a, const Polygon2d& poly_b, const Result& result) {
  char inside = ' ';
  uint fa = -1, fb = -1; // two edges with the first intersection
  uint ia = 0, ib = 0;

  double2 pa = poly_a.back(), pb = poly_b.back();
  double2 a = poly_a[0], b = poly_b[0];

  uint iterations = 2 * (poly_a.size() + poly_b.size());
  while (iterations-- > 0) {
    if (!EarlyExit && ia == fa && ib == fb) return true;

    double2 Q = b - pb, P = a - pa, R = pa - pb;
    double d = cross(Q, P);
    double s = cross(R, Q);

    if (d < 0) {
        double t = cross(R, P);
        if (0 >= s && s >= d && 0 >= t && t >= d) {
            if (EarlyExit) return true;
            if (fa == uint(-1)) fa = ia, fb = ib;
            result(pa + P * (s / d));
            inside = 'B';
        }

        if (t >= d) {
            if (inside == 'A') result(a);
            if (++ia == poly_a.size()) ia = 0;
            pa = a;
            a = poly_a[ia];
        } else {
            if (inside == 'B') result(b);
            if (++ib == poly_b.size()) ib = 0;
            pb = b;
            b = poly_b[ib];
        }
        continue;
    }

    if (d > 0 && 0 <= s && s <= d) {
        double t = cross(R, P);
        if (0 <= t && t <= d) {
            if (EarlyExit) return true;
            if (fa == uint(-1)) fa = ia, fb = ib;
            result(pa + P * (s / d));
            inside = 'A';
        }
    }

    if (s > d) {
        if (inside == 'A') result(a);
        if (++ia == poly_a.size()) ia = 0;
        pa = a;
        a = poly_a[ia];
    } else {
        if (inside == 'B') result(b);
        if (++ib == poly_b.size()) ib = 0;
        pb = b;
        b = poly_b[ib];
    }
  }
  return false;
}

template<bool EarlyExit, typename Result>
bool ConvexIntersectionGeneric2(const Polygon2d& poly_a, const Polygon2d& poly_b, const Result& result) {
  char inside = ' ';
  uint fa = -1, fb = -1; // two edges with the first intersection
  uint ia = 0, ib = 0;

  double2 a = poly_a[ia], b = poly_b[ib];
  segment2 ea(poly_a.back(), a);
  segment2 eb(poly_b.back(), b);

  const uint max_iterations = 2 * (poly_a.size() + poly_b.size());
  for (uint iteration = 0; iteration < max_iterations; iteration++) {
    if (!EarlyExit && ia == fa && ib == fb) return true;

    double2 common;
    bool intersecting = false;
    double2 R = ea.a - eb.a, Q = eb.b - eb.a, P = ea.b - ea.a;
    double d = cross(Q, P);

    if (d > 0) {
        double s = cross(R, Q) / d;
        double t = cross(R, P) / d;
        if (0 <= s && s <= 1 && 0 <= t && t <= 1) {
            intersecting = true;
            common = ea.linear(s);
        }
    }
    if (d < 0) {
        double s = cross(R, Q) / d;
        double t = cross(R, P) / d;
        if (0 <= s && s <= 1 && 0 <= t && t <= 1) {
            intersecting = true;
            common = ea.linear(s);
        }
    }

    if (intersecting) {
        if (EarlyExit) return true;
        if (fa == uint(-1)) {
            fa = ia;
            fb = ib;
        }
        result(common);
        inside = is_inside_of(a, eb) ? 'A' : 'B';
    }

    // Advance either ia or ib.
    if ((d >= 0 && !is_inside_of(a, eb)) || (d < 0 && is_inside_of(b, ea))) {
        // Advance ia.
        if (inside == 'A') result(a);
        if (++ia == poly_a.size()) ia = 0;
        a = poly_a[ia];
        ea = segment2(ea.b, a);
    } else {
        // Advance ib.
        if (inside == 'B') result(b);
        if (++ib == poly_b.size()) ib = 0;
        b = poly_b[ib];
        eb = segment2(eb.b, b);
    }
  }
  return false;
}

Polygon2d ConvexIntersection(const Polygon2d& a, const Polygon2d& b) {
    Polygon2d result;
    if (ConvexIntersectionGeneric<false>(a, b, [&](double2 p) { result.push_back(p); })) return result;
    // Boundaries are not intersecting.
    if (contains(b, a[0])) return a;
    if (contains(a, b[0])) return b;
    return {};
}

bool ConvexIntersects(const Polygon2d& a, const Polygon2d& b) {
    return ConvexIntersectionGeneric<true>(a, b, [](double2){}) || contains(b, a[0]) || contains(a, b[0]);
}

double Area(const Polygon2d& a) {
    double acc = 0;
    for (size_t j = a.size() - 1, i = 0; i < a.size(); j = i++)
        acc += signed_double_edge_area(a[j], a[i]);
    return std::abs(acc / 2);
}

template<bool OverUnion>
double ConvexIntersectionArea(const Polygon2d& a, const Polygon2d& b) {
    double acc = 0;
    bool has_first = false;
    double2 first;
    double2 prev;
    const auto area_fn = [&](double2 p) {
        if (!has_first) {
            first = prev = p;
            has_first = true;
            return;
        }
        acc += signed_double_edge_area(prev, p);
        prev = p;
    };

    if (ConvexIntersectionGeneric<false>(a, b, area_fn)) {
        acc += signed_double_edge_area(prev, first);
        const double int_area = std::abs(acc) / 2;
        if (OverUnion) return int_area / (Area(a) + Area(b) - int_area);
        return int_area;
    }
    // Boundaries are not intersecting.
    if (contains(b, a[0])) return OverUnion ? Area(a) / Area(b) : Area(a);
    if (contains(a, b[0])) return OverUnion ? Area(b) / Area(a) : Area(b);
    return 0;
}

TEST_CASE("relate", "[z]") {
    REQUIRE('X' == relate(segment2(d2(0, 0), d2(9, 0)), segment2(d2(5, -1), d2(5, 1))));
}

bool EqualRotate(const Polygon2d& a, const Polygon2d& b, uint rot) {
    if (a.size() != b.size()) return false;
    for (uint i = 0; i < a.size(); i++) if (!Equals(a[i], b[(i + rot) % b.size()])) return false;
    return true;
}

bool EqualRotate(const Polygon2d& a, const Polygon2d& b) {
    if (a.empty() && b.empty()) return true;
    for (uint i = 0; i < b.size(); i++) if (EqualRotate(a, b, i)) return true;
    return false;
}

Polygon2d FromString(std::string s) {
    Polygon2d poly;
    std::smatch m;
    #define FLOAT "([+-]?\\d+(\\.\\d*)?)"
    std::regex re("^" "\\s*" FLOAT "\\s+" FLOAT "\\s*,?");
    while (s.size() > 0) {
        Check(std::regex_search(s, m, re));
        double2 p;
        double x = parse<double>(m[1].str());
        double y = parse<double>(m[3].str());
        if (std::abs(x) < 1e-300) x = 0;
        if (std::abs(y) < 1e-300) y = 0;
        poly << d2(x, y);
        s = m.suffix();
    }
    return poly;
}

std::string ToString(const Polygon2d& poly) {
    std::string s;
    for (const auto& p : poly) s += fmt::format(s.empty() ? "{} {}" : ", {} {}", double(p.x), double(p.y));
    return s;
}

void TestIntersection(const Polygon2d& a, const Polygon2d& b, const Polygon2d& expected) {
    Polygon2d intersection = ConvexIntersection(a, b);
    std::cout.flush();
    if (!EqualRotate(expected, intersection)) {
        fmt::print("a        {}\nb        {}\nexpected {}\nactual   {}\n", ToString(a), ToString(b), ToString(expected), ToString(intersection));
        REQUIRE(false);
    }
}

void TestIntersectionSweep(Polygon2d a, Polygon2d b, const Polygon2d& expected) {
    for (uint ia = 0; ia < a.size(); ia++) {
        for (uint ib = 0; ib < b.size(); ib++) {
            TestIntersection(a, b, expected);
            TestIntersection(b, a, expected);
            std::rotate(b.begin(), b.begin() + 1, b.end());
        }
        std::rotate(a.begin(), a.begin() + 1, a.end());
    }
}

/*TEST_CASE("convex_intersection debug", "[z]") {
    Polygon2d a = FromString("0.0 0.0, 2.0 0.0, 2.0 0.0, 0.0 0.0");
    Polygon2d b = FromString("1.0 0.0, 3.0 0.0, 3.0 0.0, 1.0 0.0");
    Polygon2d expected = FromString("2 1, 2 2, 1 2, 1 1");
    TestIntersection(a, b, expected);
}*/

TEST_CASE("convex_intersection - no intersection", "[z]") {
    Polygon2d a = FromString("0 0, 2 0, 2 2, 0 2");
    Polygon2d b = FromString("3 0, 5 0, 5 2, 3 2");
    Polygon2d expected = FromString("");
    TestIntersectionSweep(a, b, expected);
}

TEST_CASE("convex_intersection - corner intersection", "[z]") {
    Polygon2d a = FromString("0 0, 2 0, 2 2, 0 2");
    Polygon2d b = FromString("1 1, 3 1, 3 3, 1 3");
    Polygon2d expected = FromString("2 1, 2 2, 1 2, 1 1");
    TestIntersectionSweep(a, b, expected);
}

TEST_CASE("convex_intersection - cross", "[z]") {
    Polygon2d a = FromString("0 0, 9 0, 9 1, 0 1");
    Polygon2d b = FromString("4 -3, 5 -3, 5 3, 4 3");
    Polygon2d expected = FromString("4 0, 5 0, 5 1, 4 1");
    TestIntersectionSweep(a, b, expected);
}

/*TEST_CASE("convex_intersection - identical", "[z]") {
    Polygon2d a = FromString("0 0, 9 0, 9 1, 0 1");
    Polygon2d b = FromString("0 0, 9 0, 9 1, 0 1");
    Polygon2d expected = FromString("0 0, 9 0, 9 1, 0 1");
    TestIntersectionSweep(a, b, expected);
}

TEST_CASE("convex_intersection - one inside other", "[z]") {
    Polygon2d a = FromString("0 0, 3 0, 3 3, 0 3");
    Polygon2d b = FromString("1 1, 2 1, 2 2, 1 2");
    Polygon2d expected = FromString("0 0, 3 0, 3 3, 0 3");
    TestIntersectionSweep(a, b, expected);
}

TEST_CASE("convex_intersection - shared edge, no intersection", "[z]") {
    Polygon2d a = FromString("0 0, 3 0, 3 3, 0 3");
    Polygon2d b = FromString("3 0, 6 0, 6 3, 3 3");
    Polygon2d expected = FromString("3 0, 3 3");
    TestIntersectionSweep(a, b, expected);
}

TEST_CASE("convex_intersection - shared vertex, no intersection", "[z]") {
    Polygon2d a = FromString("0 0, 3 0, 3 3, 0 3");
    Polygon2d b = FromString("3 3, 6 3, 6 6, 3 6");
    Polygon2d expected = FromString("3 3");
    TestIntersectionSweep(a, b, expected);
}

TEST_CASE("convex_intersection - partial edge overlap with intersection", "[z]") {
    Polygon2d a = FromString("0 0, 3 0, 3 3, 0 3");
    Polygon2d b = FromString("1 0, 2 0, 2 3, 1 3");
    Polygon2d expected = FromString("1 0, 2 0, 2 3, 1 3");
    TestIntersectionSweep(a, b, expected);
}*/
