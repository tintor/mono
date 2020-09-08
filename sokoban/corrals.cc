#include "sokoban/corrals.h"

bool is_single_component(const Level* level, const Corral& corral) {
    AgentVisitor reachable(level);
    for (size_t i = 0; i < corral.size(); i++)
        if (corral[i]) {
            reachable.add(i);
            for (const Cell* a : reachable)
                for (auto [_, b] : a->moves)
                    if (corral[b->id]) reachable.add(b);
            break;
        }
    for (size_t i = 0; i < corral.size(); i++)
        if (corral[i] && !reachable.visited(i)) return false;
    return true;
}
