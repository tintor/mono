#pragma once
#include <string>
#include <string_view>
#include <regex>
#include <sstream>
#include <array>

#include "core/algorithm.h"
#include "core/span.h"
//#include "core/util.h"

template <typename Number>
Number parse(std::string_view s) {
    std::string ss(s);
    std::stringstream is(ss);
    Number out;
    is >> out;
    return out;
}

template <typename Number>
Number parse(std::pair<const char*, const char*> s) {
    std::string ss(s.first, s.second - s.first);
    std::stringstream is(ss);
    Number out;
    is >> out;
    return out;
}

inline bool search(std::string_view s, const std::regex& re) { return std::regex_search(s.begin(), s.end(), re); }

inline bool search(std::string_view s, const std::regex& re, std::cmatch& match) {
    return std::regex_search(s.begin(), s.end(), /*out*/ match, re);
}

inline bool match(std::string_view s, const std::regex& re) { return std::regex_match(s.begin(), s.end(), re); }

inline bool match(std::string_view s, const std::regex& re, std::cmatch& match) {
    return std::regex_match(s.begin(), s.end(), /*out*/ match, re);
}

inline std::vector<std::string_view> split(std::string_view s, cspan<char> delim, bool remove_empty = true) {
    std::vector<std::string_view> out;
    const char* b = s.begin();
    int c = 0;
    for (const char* i = s.begin(); i != s.end(); i++) {
        if (contains(delim, *i)) {
            if (c > 0 || !remove_empty) out.push_back(std::string_view(b, c));
            b = i + 1;
            c = 0;
        } else {
            c += 1;
        }
    }
    if (c > 0 || !remove_empty) out.push_back(std::string_view(b, c));
    return out;
}

inline std::vector<std::string_view> split(std::string_view s, char delim = ' ', bool remove_empty = true) {
    std::array<char, 1> d = {delim};
    return split(s, d, remove_empty);
}

inline bool is_digit(char c) { return '0' <= c && c <= '9'; }

// TODO make it work for large numbers
inline bool natural_less(std::string_view a, std::string_view b) {
    auto ai = a.begin(), bi = b.begin();
    while (ai != a.end() && bi != b.end()) {
        bool an = false, bn = false;
        size_t av, bv;

        if (is_digit(*ai)) {
            an = true;
            av = 0;
            while (ai != a.end() && is_digit(*ai)) {
                av = av * 10 + (*ai - '0');
                ai++;
            }
        } else
            av = *ai++;

        if (is_digit(*bi)) {
            bn = true;
            bv = 0;
            while (bi != b.end() && is_digit(*bi)) {
                bv = bv * 10 + (*bi - '0');
                bi++;
            }
        } else
            bv = *bi++;

        if (an && !bn) av = '0';
        if (!an && bn) bv = '0';
        if (av != bv) return av < bv;
    }
    return ai == a.end() && bi != b.end();
}

inline std::string cat(std::string_view a, std::string_view b) {
    std::string s;
    s.resize(a.size() + b.size());
    copy(a.begin(), a.end(), s.begin());
    copy(b.begin(), b.end(), s.begin() + a.size());
    return s;
}

inline std::string cat(std::string_view a, const std::string& b) {
    std::string s;
    s.resize(a.size() + b.size());
    copy(a.begin(), a.end(), s.begin());
    copy(b.begin(), b.end(), s.begin() + a.size());
    return s;
}
