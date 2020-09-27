#pragma once
#include "sokoban/level.h"
#include "sokoban/util.h"

template <typename Boxes>
string_view Emoji(const Level* level, Agent agent, const Boxes& boxes, uint xy, const Boxes& frozen,
                  function<string_view(Cell*)> fn) {
    if (level->buffer[xy] == '#') return "✴️ ";
    if (level->buffer[xy] == 'e') return "  ";

    Cell* c = GetCell(level, xy);
    string_view e = fn(c);
    if (e != "") return e;

    if (c->id == agent) return c->goal ? "😎" : "😀";
    if (!c->alive) return "🌀";
    if (boxes[c->id]) {
        if (c->goal) return frozen[c->id] ? "Ⓜ️ " : "🔵";
        return "🔴";
    }
    if (c->goal) return "🏳 ";
    if (c->sink) return "🏴";
    return "🕸️ ";
}

template <typename State>
void Print(const Level* level, const State& key, function<string_view(Cell*)> fn = [] LAMBDA("")) {
    auto frozen = goals_with_frozen_boxes(level->cells[key.agent], key.boxes, level->goals());
    for (uint xy = 0; xy < level->buffer.size(); xy++) {
        print(Emoji(level, key.agent, key.boxes, xy, frozen, fn));
        if (xy % level->width == level->width - 1) print("\n");
    }
}

uint CellCount(string_view filename);
void PrintInfo(const Level* level);
