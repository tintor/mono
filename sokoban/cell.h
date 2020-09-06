#pragma once
#include "core/numeric.h"

struct Level;
struct Cell;

using Exits = std::vector<std::tuple<const Cell*, const Cell*, int>>;

struct Cell {
    const Level* level;
    int id;
    int xy;
    bool goal;
    bool sink;
    bool alive;

    int dead_region_id; // if alive then same as id, otherwise min id of all cells in dead region
    Exits exits;

    std::array<Cell*, 4> _dir;
    Cell* dir(int d) const { return _dir[d & 3]; }

    std::vector<std::pair<int, Cell*>> moves;
    std::vector<std::pair<Cell*, Cell*>> pushes;  // (box_dest, agent_src)

    std::array<Cell*, 8> dir8;

    constexpr static uint Inf = std::numeric_limits<int>::max();
    std::vector<uint> push_distance;  // from any alive cell to any goal cell (or Inf if not reachable)
    uint min_push_distance;      // min(push_distance)

    bool straight() const { return moves.size() == 2 && (moves[0].first ^ 2) == moves[1].first; }
};
