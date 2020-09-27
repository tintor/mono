#pragma once
#include "sokoban/boxes.h"

// TODO get rid of State!
template <typename TBoxes>
struct TState {
    using Boxes = TBoxes;

    Boxes boxes;
    Agent agent;

    TState() {}
    TState(Agent agent, Boxes boxes) : boxes(std::move(boxes)), agent(agent) {}

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

    bool operator==(const StateInfo& o) const {
        return distance == o.distance && heuristic == o.heuristic && dir == o.dir && closed == o.closed && prev_agent == o.prev_agent; // && prev == o.prev;
    }

    void print() const {
        ::print("distance {}, heuristic {}, dir {}, closed {}, prev_agent {}\n", distance, heuristic, int(dir), closed, prev_agent);
    }
};
static_assert(sizeof(StateInfo) == 8);
