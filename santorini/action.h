#pragma once
#include "fmt/core.h"
#include "santorini/coord.h"

struct NextStep {};
struct PlaceStep {
    Coord dest;
};
struct MoveStep {
    Coord src, dest;
};
struct BuildStep {
    Coord dest;
    bool dome;
};

using Step = std::variant<NextStep, PlaceStep, MoveStep, BuildStep>;

// for std::visit and std::variant
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;  // not needed as of C++20

inline void Print(const Step& step) {
    std::visit(overloaded{[&](NextStep a) { fmt::print("next"); },
                          [&](PlaceStep a) { fmt::print("place:{}{}", a.dest.x(), a.dest.y()); },
                          [&](MoveStep a) { fmt::print("move:{}{}:{}{}", a.src.x(), a.src.y(), a.dest.x(), a.dest.y()); },
                          [&](BuildStep a) { fmt::print("build:{}{}:{}", a.dest.x(), a.dest.y(), a.dome ? 'D' : 'T'); }},
               step);
}

using Action = std::vector<Step>;

inline void Print(const Action& action) {
    for (const Step& step : action) {
        Print(step);
        fmt::print(" ");
    }
    fmt::print("\n");
}
