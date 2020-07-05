#pragma once
#include <iostream>

#include "core/exception.h"
#include "core/small_bfs.h"

#include "sokoban/cell.h"
#include "sokoban/code.h"
#include "sokoban/state.h"
#include "sokoban/util.h"

struct Level {
    std::string name;
    int width;            // for printing only
    std::vector<char> buffer;  // xy -> code, for printing only

    // TODO unique_ptr<Cell>
    std::vector<Cell*> cells;  // ordinal -> Cell*, goals first, then alive, then dead cells
    cspan<Cell*> alive() const { return cspan<Cell*>(cells.data(), num_alive); }
    cspan<Cell*> goals() const { return cspan<Cell*>(cells.data(), num_goals); }

    int num_goals;
    int num_alive;
    int num_boxes;

    DynamicState start;
};

inline Cell* GetCell(const Level* level, uint xy) {
    for (Cell* c : level->cells)
        if (c->xy == xy) return c;
    THROW(runtime_error, "xy = %s", xy);
}

template <typename Boxes>
std::string_view Emoji(const Level* level, Agent agent, const Boxes& boxes, uint xy, const Boxes& frozen,
                  std::function<std::string_view(Cell*)> fn) {
    if (level->buffer[xy] == Code::Wall) return "✴️ ";
    if (level->buffer[xy] == 'e') return "  ";

    Cell* c = GetCell(level, xy);
    std::string_view e = fn(c);
    if (e != "") return e;

    if (c->id == agent) return c->goal ? "😎" : "😀";
    if (!c->alive) return "🌀";
    if (boxes[c->id]) {
        if (c->goal) return frozen[c->id] ? "Ⓜ️ " : "🔵";
        return "🔴";
    }
    if (c->goal) return "🏳 ";
    return "🕸️ ";
}

template <typename State>
void Print(const Level* level, const State& key, std::function<std::string_view(Cell*)> fn = [] LAMBDA("")) {
    small_bfs<const Cell*> visitor(level->cells.size());
    auto frozen = goals_with_frozen_boxes(level->cells[key.agent], key.boxes, level->goals(), visitor);
    for (uint xy = 0; xy < level->buffer.size(); xy++) {
        std::cout << Emoji(level, key.agent, key.boxes, xy, frozen, fn);
        if (xy % level->width == level->width - 1) std::cout << std::endl;
    }
}

int NumberOfLevels(std::string_view filename);
const Level* LoadLevel(std::string_view filename);
uint CellCount(std::string_view filename);
void PrintInfo(const Level* level);
