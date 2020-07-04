#pragma once
#include <string>
#include <string_view>
#include <array>
#include <fmt/core.h>
#include <fmt/format.h>

#include "core/numeric.h"
#include "core/check.h"
#include "core/span.h"

using dim_t = uint;

struct dim4 {
    dim4(dim_t a = 0, dim_t b = 0, dim_t c = 0, dim_t d = 0, char an = ' ', char bn = ' ', char cn = ' ', char dn = ' ')
            : d{a, b, c, d}, n{an, bn, cn, dn} {
        Check(a != 0 || (b == 0 && an == ' '));
        Check(b != 0 || (c == 0 && bn == ' '));
        Check(c != 0 || (d == 0 && cn == ' '));
        Check(d != 0 || dn == ' ');
    }

    int ndims() const {
        if (d[3] != 0) return 4;
        if (d[2] != 0) return 3;
        if (d[1] != 0) return 2;
        if (d[0] != 0) return 1;
        return 0;
    }

    size_t elements() const {
        if (d[3] != 0) return size_t(d[0]) * size_t(d[1]) * size_t(d[2]) * size_t(d[3]);
        if (d[2] != 0) return size_t(d[0]) * size_t(d[1]) * size_t(d[2]);
        if (d[1] != 0) return size_t(d[0]) * size_t(d[1]);
        if (d[0] != 0) return d[0];
        return 1;
    }

    char name(uint i) const { Check(i < 4 && n[i] != 0); return n[i]; }
    dim_t operator[](uint i) const { Check(i < 4 && n[i] != 0); return d[i]; }
    bool operator==(dim4 o) const { return d == o.d && n == o.n; }
    bool operator!=(dim4 o) const { return d != o.d || n != o.n; }

    dim_t back() const { Check(d[0] != 0); return d[ndims() - 1]; }

    dim4 set(uint i, dim_t a, char an = ' ') const {
        Check(i < 4 && d[i] != 0);
        dim4 e = *this;
        e.d[i] = a;
        e.n[i] = an;
        return e;
    }

    dim4 pop_front() const { return {d[1], d[2], d[3], 0, n[1], n[2], n[3], ' '}; }

    dim4 pop_back() const {
        if (d[0] == 0) return *this;
        dim4 e = *this;
        auto size = ndims();
        e.d[size - 1] = 0;
        e.n[size - 1] = ' ';
        return e;
    }

    dim4 push_front(dim_t a, char an = ' ') const {
        Check(d[3] == 0);
        return {a, d[0], d[1], d[2], an, n[0], n[1], n[2]};
    }

    dim4 push_back(dim_t a, char an = ' ') const {
        Check(d[3] == 0);
        Check(a > 0);
        dim4 e = *this;
        auto size = ndims();
        e.d[size] = a;
        e.n[size] = an;
        return e;
    }

    dim4 normalized() const {
        dim4 e;
        int w = 0;
        for (int i = 0; i < 4; i++) if (d[i] > 1) e.d[w++] = d[i];
        return e;
    }

    std::string_view dims() const { return std::string_view(&n[0], ndims()); }

    std::string str() const;

private:
    std::array<dim_t, 4> d;
    std::array<char, 4> n;
};

template <>
struct fmt::formatter<dim4> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const dim4& d, FormatContext& ctx) {
    auto s = ctx.out();
    s = fmt::format_to(s, "[");
    for (int i = 0; i < 4 && d[i] != 0; i++) {
        if (i != 0) s = fmt::format_to(s, " ");
        s = fmt::format_to(s, "{}", d[i]);
        if (d.name(i) != ' ') s = fmt::format_to(s, "{}", d.name(i));
    }
    s = fmt::format_to(s, "]");
    return s;
  }
};

inline std::string dim4::str() const { return fmt::format("{}", *this); }
