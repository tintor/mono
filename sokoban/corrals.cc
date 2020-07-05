#include "sokoban/corrals.h"

bool is_single_component(const Level* level, const Corral& corral) {
    // optimize: memory allocation
    small_bfs<Cell*> reachable(corral.size());
    for (size_t i = 0; i < corral.size(); i++)
        if (corral[i]) {
            reachable.add(level->cells[i], i);
            for (Cell* a : reachable)
                for (auto [_, b] : a->moves)
                    if (corral[b->id]) reachable.add(b, b->id);
            break;
        }
    for (size_t i = 0; i < corral.size(); i++)
        if (corral[i] && !reachable.visited[i]) return false;
    return true;
}
