#pragma once
#include "core/array_bool.h"
#include "core/bits.h"
#include "core/exception.h"
#include "core/murmur3.h"
#include "core/fmt.h"
#include "sokoban/cell.h"
#include "sokoban/common.h"

using Agent = uint;

inline bool equal(const vector<char>& a, const vector<char>& b) {
    size_t m = std::min(a.size(), b.size());
    if (!std::equal(a.begin(), a.begin() + m, b.begin())) return false;

    for (size_t i = m; i < a.size(); i++) if (a[i]) return false;
    for (size_t i = m; i < b.size(); i++) if (b[i]) return false;
    return true;
}

inline bool contains(const vector<char>& a, const vector<char>& b) {
    for (int i = 0; i < b.size(); i++) {
        if (b[i] && (i >= a.size() || !a[i])) return false;
    }
    return true;
}

inline size_t hash(const vector<char>& a) {
    size_t hash = 0;
    for (size_t i = 0; i < a.size(); i++) if (a[i]) hash ^= size_t(1) << (i % 64);
    return hash;
}

struct DynamicBoxes {
    bool operator[](const Cell* a) const { return operator[](a->id); }
    void add(const Cell* a) { set(a->id); }
    void remove(const Cell* a) { reset(a->id); }
    void move(const Cell* a, const Cell* b) { remove(a); add(b); }

    void add(uint index) { set(index); }
    void remove(uint index) { reset(index); }

    bool operator[](uint index) const { return index < data.size() && data[index]; }
    void set(uint index) { if (index >= data.size()) data.resize(index + 1); data[index] = 1; }
    void reset(uint index) { if (index < data.size()) data[index] = 0; }
    void reset() { data.clear(); }
    size_t hash() const { return ::hash(data); }
    bool operator==(const DynamicBoxes& o) const { return equal(data, o.data); }
    bool contains(const DynamicBoxes& o) const { return ::contains(data, o.data); }
    template <typename Boxes>
    operator Boxes() const {
        Boxes out;
        for (uint i = 0; i < data.size(); i++)
            if (data[i]) out.set(i);
        return out;
    }
    void print() {}
   private:
    vector<char> data;
};

template <int Words>
struct DenseBoxes {
    bool operator[](const Cell* a) const { return operator[](a->id); }
    void add(const Cell* a) { set(a->id); }
    void remove(const Cell* a) { reset(a->id); }
    void move(const Cell* a, const Cell* b) { remove(a); add(b); }

    void add(uint index) { set(index); }
    void remove(uint index) { reset(index); }

    operator DynamicBoxes() {
        DynamicBoxes out;
        for (uint i = 0; i < data.size(); i++)
            if (data[i]) out.set(i);
        return out;
    }

    bool operator[](uint index) const { return index < data.size() && data[index]; }

    void set(uint index) {
        if (index >= data.size()) THROW(runtime_error, "out of range {} : {}", index, data.size());
        data.set(index);
    }

    void reset(uint index) {
        if (index >= data.size()) THROW(runtime_error, "out of range");
        data.reset(index);
    }

    void reset() { data.reset(); }

    bool operator==(const DenseBoxes& o) const { return data == o.data; }

    auto hash() const { return std::hash<decltype(data)>()(data); }

    bool contains(const DenseBoxes& o) const { return data.contains(o.data); }

    void print() {}
   private:
    array_bool<32 * Words> data;
};

using BigBoxes = DenseBoxes<32>;

namespace std {

template <typename T, size_t Size>
struct hash<array<T, Size>> {
    size_t operator()(const array<T, Size>& a) const { return MurmurHash3_x64_128(&a, sizeof(a), 0); }
};

}  // namespace std
