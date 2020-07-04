#pragma once
#include "fmt/core.h"
#include "santorini/coord.h"

struct NextAction {};
struct PlaceAction {
    Coord dest;
};
struct MoveAction {
    Coord src, dest;
};
struct BuildAction {
    Coord dest;
    bool dome;
};

using Action = std::variant<NextAction, PlaceAction, MoveAction, BuildAction>;

// for std::visit and std::variant
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;  // not needed as of C++20

void Print(const Action& action) {
    std::visit(overloaded{[&](NextAction a) { fmt::print("next"); },
                          [&](PlaceAction a) { fmt::print("place:{}{}", a.dest.x(), a.dest.y()); },
                          [&](MoveAction a) { fmt::print("move:{}{}:{}{}", a.src.x(), a.src.y(), a.dest.x(), a.dest.y()); },
                          [&](BuildAction a) { fmt::print("build:{}{}:{}", a.dest.x(), a.dest.y(), a.dome ? 'D' : 'T'); }},
               action);
}

void Print(const std::vector<Action>& actions) {
    for (const Action& action : actions) {
        Print(action);
        fmt::print(" ");
    }
    fmt::print("\n");
}
