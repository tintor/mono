#pragma once
#include "sokoban/common.h"
#include "sokoban/cell.h"
#include "sokoban/boxes.h"

struct Level {
    string name;

    int width;            // for printing only
    vector<char> buffer;  // xy -> code, for printing only
    vector<int2> initial_steps;

    // TODO unique_ptr<Cell>
    vector<Cell*> cells;  // ordinal -> Cell*, goals first (in penalty order), then alive, then dead cells
    cspan<Cell*> alive() const { return cspan<Cell*>(cells.data(), num_alive); }
    cspan<Cell*> goals() const { return cspan<Cell*>(cells.data(), num_goals); }

    vector<Cell*> goals_in_packing_order;

    int num_goals;
    int num_alive;
    int num_boxes;

    Agent start_agent;
    DynamicBoxes start_boxes;

    const Cell* cell_by_xy(int xy) const {
        for (const Cell* cell : cells) if (cell->xy == xy) return cell;
        THROW(runtime_error, "cell not found");
    }

    const Cell* cell_by_xy(int x, int y) const { return cell_by_xy(x + y * width); }
    const Cell* cell_by_xy(int2 pos) const { return cell_by_xy(pos.x, pos.y); }

    int2 cell_to_vec(const Cell* cell) const { return {cell->xy % width, cell->xy / width}; }
};

inline Cell* GetCell(const Level* level, uint xy) {
    for (Cell* c : level->cells)
        if (c->xy == xy) return c;
    THROW(runtime_error, "xy = %s", xy);
}
