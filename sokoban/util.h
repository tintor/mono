#pragma once
#include "sokoban/cell.h"
#include "sokoban/state.h"

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
Boxes goals_with_frozen_boxes(const Cell* agent, const Boxes& boxes, cspan<Cell*> goals,
                              small_bfs<const Cell*>& visitor) {
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
    visitor.clear();
    visitor.add(agent, agent->id);

    for (const Cell* a : visitor)
        for (auto [d, b] : a->moves) {
            if (visitor.visited[b->id]) continue;
            if (!b->alive || !frozen[b->id]) {
                // agent moves to B
                visitor.add(b, b->id);
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
            visitor.add(b, b->id);
            break;
        }

    return frozen;
}

template <typename Boxes>
static bool around(const Cell* z, int side, const Boxes& boxes, int s_dir) {
    const Cell* m = z->dir(s_dir + side);
    if (!m || boxes[m->id]) return false;
    m = m->dir(s_dir);
    if (!m || boxes[m->id]) return false;
    m = m->dir(s_dir);
    if (!m || boxes[m->id]) return false;
    return true;
}

template <typename Boxes>
static bool around(const Cell* z, const Boxes& boxes, int s_dir) {
    return around(z, 1, boxes, s_dir) || around(z, 3, boxes, s_dir);
}

// can agent move to C without pushing any box?
template <typename Boxes>
bool is_cell_reachable(const Cell* c, const Cell* agent, const Boxes& boxes, small_bfs<const Cell*>& visitor) {
    visitor.clear();
    visitor.add(agent, agent->id);
    for (const Cell* a : visitor)
        for (auto [_, b] : a->moves) {
            if (c == b) return true;
            if (!boxes[b->id]) visitor.add(b, b->id);
        }
    return false;
}

template <typename Boxes>
bool is_reversible_push(const Cell* agent, const Boxes& boxes, int dir, small_bfs<const Cell*>& visitor) {
    const Cell* b = agent->dir(dir);
    const Cell* c = b->dir(dir);
    if (!c || boxes[c->id]) return false;

    if (around(agent, boxes, dir) || is_cell_reachable(c, agent, boxes, visitor)) {
        Boxes boxes2 = boxes;
        boxes2.reset(b->id);
        boxes2.set(agent->id);

        const Cell* b2 = b->dir(dir ^ 2);
        const Cell* c2 = b2->dir(dir ^ 2);
        if (!c2 || boxes[c2->id]) return false;

        return around(b, boxes2, dir ^ 2) || is_cell_reachable(c2, b, boxes2, visitor);
    }
    return false;
}

// frozen box deadlock
template <typename Boxes>
bool contains_frozen_boxes(const Cell* agent, Boxes boxes, const int num_goals, const int num_alive,
                           small_bfs<const Cell*>& visitor) {
    int num_boxes = num_goals;

    visitor.clear();
    visitor.add(agent, agent->id);

    for (const Cell* a : visitor)
        for (auto [d, b] : a->moves) {
            if (visitor.visited[b->id]) continue;
            if (!boxes[b->id]) {
                // agent moves to B
                visitor.add(b, b->id);
                continue;
            }

            const Cell* c = b->dir(d);
            if (!c || !c->alive || boxes[c->id]) continue;

            boxes.reset(b->id);
            boxes.set(c->id);
            bool m = is_simple_deadlock(c, boxes);
            boxes.reset(c->id);
            if (m) {
                boxes.set(b->id);
                continue;
            }

            // agent pushes box from B to C (and box disappears)
            if (--num_boxes == 1) return false;
            visitor.clear();  // Necessary for edge cases.
            visitor.add(b, b->id);
            break;
        }

    // deadlock if any remaining box isn't on goal
    for (uint i = num_goals; i < num_alive; i++)
        if (boxes[i]) return true;
    // deadlock if any remaining goal isn't reachable
    for (uint i = 0; i < num_goals; i++)
        if (!visitor.visited[i] && !boxes[i]) return true;
    return false;
}
