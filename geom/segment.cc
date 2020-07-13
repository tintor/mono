#include "core/exception.h"
#include "geom/aabb.h"
#include "geom/segment.h"

// D - disjoint
// O - overlap
// V - vertex / vertex touch (could be colinear, but not overlapping)
// X - interior intersection
// A - T intersection: A or B is touching interior of PQ
// B - T intersection: P or Q is touching interior of AB

bool relate_abxo(segment2 p, segment2 q, double inv_p_len) {
    int sp = Sign(signed_double_area(p.a, p.b, q.a) * inv_p_len);
    int sq = Sign(signed_double_area(p.a, p.b, q.b) * inv_p_len);
    if (sp * sq > 0) return false;

    if (sp == 0 && sq == 0)  // colinear
        return Overlaps(aabb2(p), aabb2(q));

    double q_len = length(q.a - q.b);
    int sa = Sign(signed_double_area(q.a, q.b, p.a) / q_len);
    int sb = Sign(signed_double_area(q.a, q.b, p.b) / q_len);
    if (sa * sb == 0) return sp * sq < 0;
    return sa * sb < 0;
}

bool relate_abxo(segment2 p, segment2 q) {
    double p_len = length(p.a - p.b);
    int sp = Sign(signed_double_area(p.a, p.b, q.a) / p_len);
    int sq = Sign(signed_double_area(p.a, p.b, q.b) / p_len);
    if (sp * sq > 0) return false;

    if (sp == 0 && sq == 0)  // colinear
        return Overlaps(aabb2(p), aabb2(q));

    double q_len = length(q.a - q.b);
    int sa = Sign(signed_double_area(q.a, q.b, p.a) / q_len);
    int sb = Sign(signed_double_area(q.a, q.b, p.b) / q_len);
    if (sa * sb == 0) return sp * sq < 0;
    return sa * sb < 0;
}

int relate_full(segment2 p, segment2 q, double* pt, double* qt) {
    THROW(not_implemented);
    /*	int sp = Sign(p, q.a);
            int sq = Sign(p, q.b);
            if (sp == 0 && sq == 0) { // colinear
                    if (Overlaps(aabb2(p), aabb2(q))) { // TODO overlaps must respect tolerances
                            int result = COLINEAR | OVERLAP;
                            // TODO set pt and qt in case of overlap! (be carefull to return both points)!
                            if (Intersects(aabb2(p), q.a))
                                    result |= QA;
                            if (Intersects(aabb2(p), q.b))
                                    result |= QB;
                            if (Intersects(aabb2(q), p.a))
                                    result |= PA;
                            if (Intersects(aabb2(q), p.b))
                                    result |= PB;
                            return result;
                    }
                    if (Equals(p.a, q.a))
                            return COLINEAR | VERTEX_VERTEX | PA | QA;
                    if (Equals(p.a, q.b))
                            return COLINEAR | VERTEX_VERTEX | PA | QB;
                    if (Equals(p.b, q.a))
                            return COLINEAR | VERTEX_VERTEX | PB | QA;
                    if (Equals(p.b, q.b))
                            return COLINEAR | VERTEX_VERTEX | PB | QB;
                    return COLINEAR; // but disjoint
            }

            if (Equals(p.a, q.a))
                    return VERTEX_VERTEX | PA | QA;
            if (Equals(p.a, q.b))
                    return VERTEX_VERTEX | PA | QB;
            if (Equals(p.b, q.a))
                    return VERTEX_VERTEX | PB | QA;
            if (Equals(p.b, q.b))
                    return VERTEX_VERTEX | PB | QB;

            int sa = Sign(q, p.a);
            int sb = Sign(q, p.b);
            int sab = sa * sb;
            int spq = sp * sq;
            if (sab < 0 && spq < 0) {
                    if (pt || st) {
                            double2 A = p.a, B = p.b, P = q.a, Q = q.b;
                            double2 AP = A - P, QP = Q - P, BA = B - A;
                            double d = cross(QP, BA);
                            if (pt)
                                    *pt = cross(AP, QP) / d;
                            if (qt)
                                    *qt = cross(AP, BA) / d;
                    }
                    return CROSS;
            }
            if (sa == 0 && spq < 0) {
                    return VERTEX_EDGE | PA;
            }
            if (sb == 0 && spq < 0)
                    return VERTEX_EDGE | PB;
            if (sab < 0 && sp == 0)
                    return EDGE_VERTEX | QA;
            if (sab < 0 && sq == 0)
                    return EDGE_VERTEX | QB;
            return 0; // disjoint*/
}

// returns a single common point
bool relate_non_parallel(segment2 p, segment2 q, double2* common) {
    int sp = Sign(p, q.a);
    int sq = Sign(p, q.b);
    if (sp == 0 && sq == 0) return false; // parallel

    if (Equals(p.a, q.a) || Equals(p.a, q.b)) {
        if (common) *common = p.a;
        return true;
    }
    if (Equals(p.b, q.a) || Equals(p.b, q.b)) {
        if (common) *common = p.b;
        return true;
    }

    int sa = Sign(q, p.a);
    int sb = Sign(q, p.b);
    int sab = sa * sb;
    int spq = sp * sq;
    if (sab < 0 && spq < 0) {
        if (common) {
            double2 R = p.a - q.a, Q = q.b - q.a, P = p.b - p.a;
            double d = cross(Q, P);
            *common = p.linear(cross(R, Q) / d);
        }
        return true;
    }
    if (sab == 0 && spq < 0) {
        if (common) *common = (sa == 0) ? p.a : p.b;
        return true;
    }
    if (sab < 0 && spq == 0) {
        if (common) *common = (sp == 0) ? q.a : q.b;
        return true;
    }
    return false;
}

// TODO(marko) test that result will be stable if params are swapped (or if segments are reversed)
char relate(segment2 p, segment2 q, double2* pt, double2* qt) {
    int sp = Sign(p, q.a);
    int sq = Sign(p, q.b);
    if (sp == 0 && sq == 0) {  // colinear
        if (Overlaps(aabb2(p), aabb2(q))) {
            if (pt) {
                double t = p.param(q.a);
                double s = p.param(q.b);
                *pt = (t < s) ? double2{t, s} : double2{s, t};
            }
            if (qt) {
                double t = q.param(p.a);
                double s = q.param(p.b);
                *qt = (t < s) ? double2{t, s} : double2{s, t};
            }
            return 'O';
        }

        if (Equals(p.a, q.a)) {
            if (pt) *pt = double2{0, 0};
            if (qt) *qt = double2{0, 0};
            return 'V';
        }
        if (Equals(p.a, q.b)) {
            if (pt) *pt = double2{0, 0};
            if (qt) *qt = double2{1, 1};
            return 'V';
        }
        if (Equals(p.b, q.a)) {
            if (pt) *pt = double2{1, 1};
            if (qt) *qt = double2{0, 0};
            return 'V';
        }
        if (Equals(p.b, q.b)) {
            if (pt) *pt = double2{1, 1};
            if (qt) *qt = double2{1, 1};
            return 'V';
        }
        return 'D';
    }

    if (Equals(p.a, q.a)) {
        if (pt) *pt = double2{0, 0};
        if (qt) *qt = double2{0, 0};
        return 'V';
    }
    if (Equals(p.a, q.b)) {
        if (pt) *pt = double2{0, 0};
        if (qt) *qt = double2{1, 1};
        return 'V';
    }
    if (Equals(p.b, q.a)) {
        if (pt) *pt = double2{1, 1};
        if (qt) *qt = double2{0, 0};
        return 'V';
    }
    if (Equals(p.b, q.b)) {
        if (pt) *pt = double2{1, 1};
        if (qt) *qt = double2{1, 1};
        return 'V';
    }

    int sa = Sign(q, p.a);
    int sb = Sign(q, p.b);
    int sab = sa * sb;
    int spq = sp * sq;
    if (sab < 0 && spq < 0) {
        if (pt || qt) {
            double2 R = p.a - q.a, Q = q.b - q.a, P = p.b - p.a;
            double d = cross(Q, P);
            if (pt) *pt = broad2(cross(R, Q) / d);
            if (qt) *qt = broad2(cross(R, P) / d);
        }
        return 'X';
    }
    if (sab == 0 && spq < 0) {
        if (pt) *pt = (sa == 0) ? double2{0, 0} : double2{1, 1};
        if (qt) {
            double2 R = p.a - q.a, Q = q.b - q.a, P = p.b - p.a;
            *qt = broad2(cross(R, P) / cross(Q, P));
        }
        return 'A';
    }
    if (sab < 0 && spq == 0) {
        if (pt) {
            double2 R = p.a - q.a, Q = q.b - q.a, P = p.b - p.a;
            *pt = broad2(cross(R, Q) / cross(Q, P));
        }
        if (qt) *qt = (sp == 0) ? double2{0, 0} : double2{1, 1};
        return 'B';
    }
    return 'D';
}

bool relate(segment2 p, ray2 q) {
    int sp = Sign(p, q.origin);
    int sq = Sign(p, q.origin + q.unit_dir);
    if (sp == 0 && sq == 0) {  // colinear
        if (Overlaps(aabb2(p), aabb2(q.origin, q.infinity()))) return 'O';
        if (Equals(p.a, q.origin) || Equals(p.b, q.origin)) return 'V';
        return 'D';
    }

    if (Equals(p.a, q.origin) || Equals(p.b, q.origin)) return 'V';

    int sab = Sign(q, p.a) * Sign(q, p.b);
    int spq = sp * sq;
    if (sab < 0 && spq < 0) return 'X';
    if (sab == 0 && spq < 0) return 'A';
    if (sab < 0 && spq == 0) return 'B';
    return 'D';
}

std::pair<segment3, NearestCase> nearest(segment3 p, segment3 q) {
    double3 A = p.b - p.a, B = q.b - q.a, C = p.a - q.a;
    double aa = dot(A, A), bb = dot(B, B), ab = dot(A, B), ac = dot(A, C), bc = dot(B, C);
    constexpr double tiny = 1e-8;

    // ray/ray
    double d = aa * bb - ab * ab;
    double s = ab * bc - bb * ac;
    double t = aa * bc - ab * ac;
    // Note: [d >= tiny * aa * bb] is needed to make parallelness test indepent of line lengths
    if ((d >= tiny && d >= tiny * aa * bb && 0 <= s && s <= d && 0 <= t && t <= d) ||
        (d <= -tiny && d <= -tiny * aa * bb && d <= s && s <= 0 && d <= t && t <= 0))
        return {segment3(p.a + A * (s / d), q.a + B * (t / d)), NearestCase::RayRay};

    // ray/endpoint
    double s0 = (aa >= tiny) ? -ac / aa : -1;
    double s1 = (aa >= tiny) ? (ab - ac) / aa : -1;
    double t0 = (bb >= tiny) ? bc / bb : -1;
    double t1 = (bb >= tiny) ? (ab + bc) / bb : -1;

    double d1 = (0 <= s0 && s0 <= 1) ? squared(C + A * s0) : INF;
    double d2 = (0 <= s1 && s1 <= 1) ? squared(C - B + A * s1) : INF;
    double d3 = (0 <= t0 && t0 <= 1) ? squared(B * t0 - C) : INF;
    double d4 = (0 <= t1 && t1 <= 1) ? squared(B * t1 - C - A) : INF;

    // endpoint/endpoint
    double d5 = squared(C);
    double d6 = squared(C + A);
    double d7 = squared(C - B);
    double d8 = squared(C + A - B);

    double dm = std::min(min(d1, d2, d3, d4), min(d5, d6, d7, d8));

    if (d1 == dm) return {segment3(p.a + A * s0, q.a), NearestCase::RayPoint};
    if (d2 == dm) return {segment3(p.a + A * s1, q.b), NearestCase::RayPoint};
    if (d3 == dm) return {segment3(p.a, q.a + B * t0), NearestCase::RayPoint};
    if (d4 == dm) return {segment3(p.b, q.a + B * t1), NearestCase::RayPoint};
    if (d5 == dm) return {segment3(p.a, q.a), NearestCase::PointPoint};
    if (d6 == dm) return {segment3(p.b, q.a), NearestCase::PointPoint};
    if (d7 == dm) return {segment3(p.a, q.b), NearestCase::PointPoint};
    if (d8 == dm) return {segment3(p.b, q.b), NearestCase::PointPoint};

    THROW(runtime_error, "unreachable");
}

double squared_distance(segment3 p, segment3 q) {
    double3 A = p.b - p.a, B = q.b - q.a, C = p.a - q.a;
    double aa = dot(A, A), bb = dot(B, B), ab = dot(A, B), ac = dot(A, C), bc = dot(B, C);
    constexpr double tiny = 1e-8;

    // ray/ray
    double d = aa * bb - ab * ab;
    double s = ab * bc - bb * ac;
    double t = aa * bc - ab * ac;
    // Note: [d >= tiny * aa * bb] is needed to make parallelness test indepent of line lengths
    if ((d >= tiny && d >= tiny * aa * bb && 0 <= s && s <= d && 0 <= t && t <= d) ||
        (d <= -tiny && d <= -tiny * aa * bb && d <= s && s <= 0 && d <= t && t <= 0))
        return squared(C + A * (s / d) - B * (t / d));

    // ray/endpoint
    double s0 = (aa >= tiny) ? -ac / aa : -1;
    double s1 = (aa >= tiny) ? (ab - ac) / aa : -1;
    double t0 = (bb >= tiny) ? bc / bb : -1;
    double t1 = (bb >= tiny) ? (ab + bc) / bb : -1;

    double d1 = (0 <= s0 && s0 <= 1) ? squared(C + A * s0) : INF;
    double d2 = (0 <= s1 && s1 <= 1) ? squared(C - B + A * s1) : INF;
    double d3 = (0 <= t0 && t0 <= 1) ? squared(B * t0 - C) : INF;
    double d4 = (0 <= t1 && t1 <= 1) ? squared(B * t1 - C - A) : INF;

    // endpoint/endpoint
    double d5 = squared(C);
    double d6 = squared(C + A);
    double d7 = squared(C - B);
    double d8 = squared(C + A - B);

    return min(d1, d2, d3, d4, d5, d6, d7, d8);
}

double squared_distance(line3 e, double3 p) {
    double3 pa = p - e.origin;
    return squared(pa - e.unit_dir * dot(pa, e.unit_dir));
}

double distance(double3 a, double3 b) { return length(a - b); }

double distance(double3 a, segment3 b) { return distance(a, b.nearest(a)); }
double distance(segment3 a, double3 b) { return distance(b, a); }

double distance(segment3 a, segment3 b) { return sqrt(squared_distance(a, b)); }
