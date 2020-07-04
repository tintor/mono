#pragma once
#include <fmt/format.h>

#include "core/span.h"

// T can be any number-like type
template <typename T>
struct range {
    explicit range(T& end) : range(0, end) {}
    explicit range(const T& end) : range(0, end) {}

    range(T begin, T& end, T inc = 1) : _begin(begin), _end(0), _end_ptr(&end), _inc(inc) {
        assert((inc > 0 && begin <= end) || (inc < 0 && begin >= end));
    }

    range(T begin, const T& end, T inc = 1) : _begin(begin), _end(end), _end_ptr(&_end), _inc(inc) {
        assert((inc > 0 && begin <= end) || (inc < 0 && begin >= end));
    }

    template <typename V>
    range(const span<V>& v) : range(v.size()) {}

    struct iterator {
        iterator(T pos, const range& _range) : _pos(pos), _range(_range) {}

        T& operator*() { return _pos; }
        T operator*() const { return _pos; }
        void operator++() { _pos += inc(); }
        void operator++(int) { _pos += inc(); }
        void operator--() { _pos -= inc(); }
        void operator--(int) { _pos -= inc(); }
        bool operator!=(iterator) { return (inc() > 0) ? (_pos < end()) : (_pos > end()); }

       private:
        T inc() const { return _range._inc; }
        T end() const { return _range._end_ptr ? *_range._end_ptr : _range._end; }

        T _pos;
        const range& _range;
    };

    iterator begin() const { return iterator(_begin, *this); }
    iterator end() const { return iterator(_end_ptr ? *_end_ptr : _end, *this); }
    T inc() const { return _inc; }

   private:
    const T _begin;
    const T _end;
    const T* _end_ptr;
    const T _inc;
};

template <typename T>
struct fmt::formatter<range<T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const range<T>& v, FormatContext& ctx) {
    auto s = ctx.out();
    using namespace std::literals;
    s = fmt::format_to(s, "range(");
    if (*v.begin() != 0 || std::abs(v.inc()) != 1) {
        s = fmt::format_to(s, "{}, ", *v.begin());
    }
    const auto ei = v.end();
    s = fmt::format_to(s, "{}", *ei);
    if (std::abs(v.inc()) != 1) {
        s = fmt::format_to(s, ", {}", v.inc());
    }
    s = fmt::format_to(s, ")");
    return s;
  }
};
