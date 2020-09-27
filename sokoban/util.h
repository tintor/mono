#pragma once
#include "sokoban/agent_visitor.h"
#include "sokoban/agent_box_visitor.h"

template <typename Boxes>
inline bool free(const Cell* a, const Boxes& boxes) {
    return a && !boxes[a->id];
}

//      ##
// TODO #*
//      $$
//      #*
//      ##
// * are dead cells

// $$  $#  $#
// $$  $#  $$
template <typename Boxes>
static bool is_2x2_deadlock(const Cell* box, const Boxes& boxes) {
    for (int d = 0; d < 4; d++) {
        const Cell* a = box->dir(d);
        if (free(a, boxes)) continue;
        const Cell* b = box->dir(d + 1);
        if (free(b, boxes)) continue;
        if (!a && !b) return !box->goal;
        if (a) {
            const Cell* c = a->dir(d + 1);
            if (!free(c, boxes)) return !(box->goal && a->goal && (!b || b->goal) && (!c || c->goal));
        }
        if (b) {
            const Cell* c = b->dir(d);
            if (!free(c, boxes)) return !(box->goal && b->goal && (!a || a->goal) && (!c || c->goal));
        }
    }
    return false;
}

// #$.
// .$#
template <typename Boxes>
static bool is_2x3_deadlock(const Cell* pushed_box, const Boxes& boxes) {
    const Cell* a = pushed_box;
    for (int d = 0; d < 4; d++) {
        const Cell* b = a->dir(d);
        if (!b || !boxes[b->id]) continue;
        if (a->goal && b->goal) continue;
        // Both A and B are boxes, and one of them is not on goal
        if (!a->dir(d - 1) && !b->dir(d + 1)) return true;
        if (!a->dir(d + 1) && !b->dir(d - 1)) return true;
    }
    return false;
}

template <typename Boxes>
bool is_simple_deadlock(const Cell* pushed_box, const Boxes& boxes) {
    return is_2x2_deadlock(pushed_box, boxes) || is_2x3_deadlock(pushed_box, boxes);
}

template <typename Boxes>
bool is_frozen_on_goal_simple(const Cell* box, const Boxes& boxes) {
    for (int d = 0; d < 4; d++) {
        const Cell* a = box->dir(d);
        if (free(a, boxes)) continue;
        const Cell* b = box->dir(d + 1);
        if (free(b, boxes)) continue;

        if (!a && !b) return true;

        if (a) {
            Cell* c = a->dir(d + 1);
            if (!free(c, boxes)) return true;
        }
        if (b) {
            Cell* c = b->dir(d);
            if (!free(c, boxes)) return true;
        }
    }
    return false;
}

template <typename Boxes>
Boxes goals_with_frozen_boxes(const Cell* agent, const Boxes& boxes, cspan<Cell*> goals) {
    Boxes frozen;

    // try simple approach first
    bool simple = true;
    for (int g = 0; g < goals.size(); g++)
        if (boxes[g]) {
            if (is_frozen_on_goal_simple(goals[g], boxes))
                frozen.set(g);
            else
                simple = false;
        }
    if (simple) return frozen;

    // more extensive check
    // iteratively remove all boxes that agent can push from its reachable area
    frozen = boxes;
    int num_boxes = goals.size();
    AgentVisitor visitor(agent);
    for (const Cell* a : visitor)
        for (auto [d, b] : a->actions) {
            if (!b->alive || !frozen[b->id]) {
                // agent moves to B
                visitor.add(b);
                continue;
            }

            const Cell* c = b->dir(d);
            if (!c || !c->alive || frozen[c->id]) continue;

            frozen.reset(b->id);
            frozen.set(c->id);
            bool m = is_simple_deadlock(c, frozen);
            frozen.reset(c->id);
            if (m) {
                frozen.set(b->id);
                continue;
            }

            // agent pushes box from B to C (and box disappears)
            if (--num_boxes == 1) {
                frozen.reset();
                return frozen;
            }
            visitor.clear();
            visitor.add(b);
            break;
        }

    return frozen;
}

// Can agent move to C without pushing any box?
template <typename Boxes>
bool is_cell_reachable(const Cell* c, const Cell* agent, const Boxes& boxes) {
    AgentVisitor visitor(agent);
    for (const Cell* a : visitor)
        for (const Cell* b : a->new_moves) {
            if (c == b) return true;
            if (!boxes[b->id]) visitor.add(b);
        }
    return false;
}

template <typename Boxes>
const Cell* normalize(const Cell* agent, const Boxes& boxes) {
    AgentVisitor visitor(agent);
    for (const Cell* a : visitor) {
        if (a->id < agent->id) agent = a;
        for (auto [_, b] : a->moves)
            if (!boxes[b->id]) visitor.add(b);
    }
    return agent;
}

template <typename Boxes>
void normalize(const Level* level, Agent* agent, const Boxes& boxes) {
    *agent = normalize(level->cells[*agent], boxes)->id;
}

template <typename State, typename PushFn>
void for_each_push(const Level* level, const State& s, const PushFn& push) {
    AgentVisitor visitor(level, s.agent);
    for (const Cell* a : visitor) {
        for (auto [d, b] : a->actions) {
            if (!s.boxes[b->id]) {
                visitor.add(b);
                continue;
            }
            const Cell* c = b->dir(d);
            // TODO remove c->sink
            if (c && (c->alive || c->sink) && !s.boxes[c->id]) push(a, b, d);
        }
    }
}

using Continue = Symbol<symbol_hash("continue")>;
using Break = Symbol<symbol_hash("break")>;
using Deadlock = Symbol<symbol_hash("deadlock")>;

// Can push the same box multiple times!
template <typename Boxes, typename Out>
variant<Continue, Break> for_each_multi_push(const Level* level, const Cell* agent, Boxes boxes, const Cell* dont_select_box, const Out& out) {
    AgentVisitor reachable(agent);
    for (const Cell* a : reachable) {
        for (auto [_, b] : a->moves) {
            if (!boxes[b->id]) reachable.add(b);
        }
    }

    AgentBoxVisitor visitor(level);
    for (const Cell* sb : level->alive()) {
        if (!boxes[sb] || sb == dont_select_box) continue;

        boxes.remove(sb);
        visitor.clear();
        for (auto [d, a] : sb->moves) {
            if (reachable.visited(a)) {
                const Cell* c = sb->dir(d ^ 2);
                if (c && c->alive) {
                    boxes.add(c);
                    visitor.add(normalize(sb, boxes), c);
                    boxes.remove(c);
                }
            }
        }

        for (const auto [v_agent, v_box] : visitor) {
            variant<Continue, Break, Deadlock> result = out(sb, v_agent, v_box);
            if (result == Break()) return Break();
            if (result == Deadlock()) continue;

            const auto v_box_ = v_box;
            boxes.add(v_box);
            for_each_push(level, TState(v_agent->id, boxes), [&](const Cell* a, const Cell* b, int d) {
                if (b != v_box_) return;
                const Cell* c = b->dir(d);
                boxes.move(b, c);
                visitor.add(normalize(b, boxes), c);
                boxes.move(c, b);
            });
            boxes.remove(v_box);
        }
        boxes.add(sb);
    }
    return Continue();
}
