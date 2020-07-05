#pragma once
#include <array>

#include "core/murmur3.h"

template <uint Size>
struct array_bool {
    typedef uint WordType;
    static constexpr uint Words = (Size + 31) / 32;
    std::array<uint, Words> words;

    array_bool() {
        for (uint& e : words) e = 0;
    }

    array_bool(const array_bool& o) : words(o.words) {}

    constexpr static uint size() { return Size; }

    bool operator[](int index) const {
        uint mask = uint(1) << (index % 32);
        return (words[index / 32] & mask) != 0;
    }

    void set(int index) {
        uint mask = uint(1) << (index % 32);
        words[index / 32] |= mask;
    }

    void reset(int index) {
        uint mask = uint(1) << (index % 32);
        words[index / 32] &= ~mask;
    }

    void reset() {
        for (uint& w : words) w = 0;
    }

    bool contains(const array_bool& b) const {
        for (uint i = 0; i < Words; i++)
            if ((words[i] | b.words[i]) != words[i]) return false;
        return true;
    }
};

template <uint Size>
inline bool operator==(const array_bool<Size>& a, const array_bool<Size>& b) {
    for (uint i = 0; i < array_bool<Size>::Words; i++)
        if (a.words[i] != b.words[i]) return false;
    return true;
}

namespace std {
template <uint Size>
struct hash<array_bool<Size>> {
    size_t operator()(const array_bool<Size>& a) const {
        return MurmurHash3_x64_128(&a, 4 * array_bool<Size>::Words, 0);
    }
};
}  // namespace std
