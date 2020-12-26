#pragma once
#include <cassert>

constexpr int HexDigit(char hex) { return ('A' <= hex && hex <= 'F') ? (hex - 'A' + 10) : (hex - '0'); }

constexpr float HexFloat(const char hex[2]) {
    assert(hex[0] != 0 && hex[1] != 0);
    int v = HexDigit(hex[0]) * 16 + HexDigit(hex[1]);
    return v / 256.f + (0.5f / 256.f);
}

struct Color {
    constexpr Color(const char code[6]) : r(HexFloat(code)), g(HexFloat(code + 2)), b(HexFloat(code + 4)) {}
    float r, g, b;
};
