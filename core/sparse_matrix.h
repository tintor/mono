#pragma once
#include <vector>
#include <cmath>

constexpr float EPS = 1e-9;

template <typename F, typename I>
struct sparse_cell {
    I x, y;
    F v;
};

template <typename F, typename I>
bool row_order(sparse_cell<F, I> a, sparse_cell<F, I> b) {
    return (a.y == b.y) ? (a.x < b.x) : (a.y < b.y);
}

template <typename F, typename I>
bool column_order(sparse_cell<F, I> a, sparse_cell<F, I> b) {
    return (a.x == b.x) ? (a.y < b.y) : (a.x < b.x);
}

// assumed to be in row by row order
template <typename F, typename I>
using sparse_matrix = std::vector<sparse_cell<F, I>>;

template <typename F, typename I>
sparse_matrix<F, I> add(const sparse_matrix<F, I>& a, const sparse_matrix<F, I>& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    sparse_matrix<F, I> c;
    auto ai = a.begin(), bi = b.begin();
    while (ai != a.end() && bi != b.end()) {
        if (ai->x == bi->x && ai->y == bi->y) {
            auto s = ai->v + bi->v;
            if (abs(s) > EPS) c.push_back({ai->x, ai->y, s});
            ++ai;
            ++bi;
        } else if (row_order(*ai, *bi))
            c.push_back(*ai++);
        else
            c.push_back(*bi++);
    }
    std::copy(ai, a.end(), std::back_inserter(c));
    std::copy(bi, b.end(), std::back_inserter(c));
    return c;
}

template <typename F, typename I>
sparse_matrix<F, I> transpose(sparse_matrix<F, I> a) {
    for (auto& e : a) swap(e.x, e.y);
    std::sort(a.begin(), a.end(), row_order<F, I>);
    return a;
}

template <typename F, typename I>
sparse_matrix<F, I> mul(const sparse_matrix<F, I>& a, sparse_matrix<F, I> b) {
    sparse_matrix<F, I> c;
    if (a.empty() || b.empty()) return c;
    std::sort(b.begin(), b.end(), column_order<F, I>);
    auto as = a.begin();
    while (as != a.end()) {
        // [as, ae) is one row in A
        auto ae = as;
        while (ae != a.end() && as->y == ae->y) ++ae;

        // multiply [as, ae) row with the entire B
        auto ai = as;
        F s = 0;
        auto x = b[0].x;
        for (auto bi = b.begin(); bi != b.end(); ++bi) {
            if (bi->x != x) {
                if (std::abs(s) > EPS) c.push_back({x, ai->y, s});
                s = 0;
                ai = as;
                x = bi->x;
            }
            while (ai < ae && ai->x < bi->y) ++ai;
            if (ai < ae && ai->x == bi->y) s += ai->v * bi->v;
        }
        if (std::abs(s) > EPS) c.push_back({x, ai->y, s});

        as = ae;
    }
    return c;
}
