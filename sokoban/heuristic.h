#pragma once
#include "sokoban/hungarian.h"
#include "sokoban/level.h"
#include "sokoban/util.h"

template<typename Boxes>
uint heuristic_simple(const Level* level, const Boxes& boxes) {
    uint cost = 0;
    for (const Cell* box : level->alive()) {
        if (!boxes[box->id]) continue;
        cost += box->min_push_distance + box->goal_penalty;
    }
    return cost;
}

// excludes frozen goals from costs
template<typename Boxes>
uint heuristic(const Level* level, const Boxes& boxes) {
    std::vector<uint> goal;  // TODO avoid this alloc
    for (const Cell* g : level->goals())
        if (!boxes[g->id] || !is_frozen_on_goal_simple(g, boxes)) goal.push_back(g->id);
    if (goal.size() == level->num_goals) return heuristic_simple(level, boxes);

    uint cost = 0;
    for (const Cell* box : level->alive()) {
        if (!boxes[box->id]) continue;

        if (!box->goal) {
            // min push distance out of all non-frozen goals
            uint dist = Cell::Inf;
            for (uint i = 0; i < goal.size(); i++) minimize(dist, box->push_distance[goal[i]]);
            if (dist == Cell::Inf) return Cell::Inf;
            cost += dist;
        }
        cost += box->goal_penalty;
    }
    return cost;
}
