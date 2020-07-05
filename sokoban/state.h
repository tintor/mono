#pragma once
#include "core/array_bool.h"
#include "core/bits.h"
#include "core/exception.h"
//#include "core/hash.h"
#include "core/murmur3.h"

using Agent = uint;

struct DynamicBoxes {
    bool operator[](uint index) const { return data[index]; }
    void set(uint index) { data.set(index); }
    void reset(uint index) { data.reset(index); }
    void reset() { data.clear(); }
    auto hash() const { return data.hash(); }
    bool operator==(const DynamicBoxes& o) const { return data == o.data; }
    bool contains(const DynamicBoxes& o) const { return data.contains(o.data); }
    template <typename Boxes>
    operator Boxes() const {
        Boxes out;
        for (uint i = 0; i < data.size(); i++)
            if (data[i]) out.set(i);
        return out;
    }

   private:
    Bits data;
};

template <int Words>
struct DenseBoxes {
    operator DynamicBoxes() {
        DynamicBoxes out;
        for (uint i = 0; i < data.size(); i++)
            if (data[i]) out.set(i);
        return out;
    }

    bool operator[](uint index) const { return index < data.size() && data[index]; }

    void set(uint index) {
        if (index >= data.size()) THROW(runtime_error, "out of range");
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

   private:
    array_bool<32 * Words> data;
};

namespace std {

template <typename T, size_t Size>
struct hash<array<T, Size>> {
    size_t operator()(const array<T, Size>& a) const { return MurmurHash3_x64_128(&a, sizeof(a), 0); }
};

}  // namespace std

template <typename T, int Capacity>
struct SparseBoxes {
    static constexpr int MaxBoxes = std::min(2, Capacity);
    static constexpr ulong MaxIndex = std::numeric_limits<T>::max() - 1;
    static constexpr T Empty = ~T(0);
    static_assert(MaxIndex < Empty);

    operator DynamicBoxes() {
        DynamicBoxes out;
        for (uint i = 0; i < data.size(); i++)
            if (data[i]) out.set(i);
        return out;
    }

    SparseBoxes() { reset(); }

    bool operator[](uint index) const {
        if (index > MaxIndex) return false;
        for (T a : data)
            if (a >= index) return a == index;
        return false;
    }

    void set(uint index) {
        if (index > MaxIndex) THROW(runtime_error, "out of range");

        for (int i = 0; i < MaxBoxes; i++)
            if (data[i] >= index) {
                if (data[i] > index) {
                    if (data[MaxBoxes - 1] != Empty) THROW(runtime_error, "out of capacity");
                    memmove(data.data() + i + 1, data.data() + i, (MaxBoxes - i - 1) * sizeof(T));
                    data[i] = index;
                }
                return;
            }
    }

    void reset(int index) {
        if (index > MaxIndex) THROW(runtime_error, "out of range");

        for (int i = 0; i < MaxBoxes; i++)
            if (data[i] >= index) {
                if (data[i] > index) return;
                memmove(data.data() + i, data.data() + i + 1, (MaxBoxes - i - 1) * sizeof(T));
                data[MaxBoxes - 1] = Empty;
                return;
            }
    }

    bool operator==(const SparseBoxes& o) const { return data == o.data; }
    bool empty() const { return data[0] == Empty; }
    void reset() {
        for (auto& e : data) e = Empty;
    }
    auto hash() const { return std::hash<decltype(data)>()(data); }

    bool contains(const SparseBoxes& o) const {
        int sa = 0, sb = 0;
        while (true) {
            if (sa == MaxBoxes || data[sa] == Empty) return false;
            if (sb == MaxBoxes || o.data[sb] == Empty) return true;
            if (data[sa] > o.data[sb]) return false;
            if (data[sa] == o.data[sb]) sb += 1;
            sa += 1;
        }
    }

   private:
    std::array<T, MaxBoxes> data;  // in non-decreasing order
};

// TODO get rid of State!
template <typename TBoxes>
struct TState {
    using Boxes = TBoxes;

    Boxes boxes;
    Agent agent;

    TState() {}
    TState(Agent agent, const Boxes& boxes) : boxes(boxes), agent(agent) {}

    template <typename Boxes2>
    operator TState<Boxes2>() const {
        return {agent, static_cast<Boxes>(boxes)};
    }
};

template <typename TBoxes>
inline bool operator==(const TState<TBoxes>& a, const TState<TBoxes>& b) {
    return a.agent == b.agent && a.boxes == b.boxes;
}

namespace std {

template <typename TBoxes>
struct hash<TState<TBoxes>> {
    size_t operator()(const TState<TBoxes>& a) const { return a.boxes.hash() ^ fmix64(a.agent); }
};

}  // namespace std

using DynamicState = TState<DynamicBoxes>;

struct StateInfo {
    ushort distance = 0;   // pushes so far
    ushort heuristic = 0;  // estimated pushes remaining
    char dir = -1;
    bool closed = false;
    short prev_agent = -1;
};
static_assert(sizeof(StateInfo) == 8);
