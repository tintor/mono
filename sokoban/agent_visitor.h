#pragma once
#include "sokoban/level.h"

class AgentVisitor : public each<AgentVisitor> {
private:
    static constexpr int kCapacity = 1214;

    static_vector<ushort, kCapacity> _queue;
    static_vector<char, kCapacity> _visited;
    int _head = 0;
    int _tail = 0;
    const Level* _level;

public:
    AgentVisitor(const Level* level) : _level(level) {
        // TODO use absl::InlinedVector to remove this limitation
        if (level->cells.size() > kCapacity) THROW(runtime_error, "level has more cells ({}) than capacity ({})", level->cells.size(), kCapacity);
        _visited.resize(level->cells.size(), 0);
    }

    AgentVisitor(const Level* level, const int start) : AgentVisitor(level) {
        _queue[_tail++] = start;
        _visited[start] = 1;
    }

    AgentVisitor(const Cell* start) : AgentVisitor(start->level) {
        _queue[_tail++] = start->id;
        _visited[start->id] = 1;
    }

    bool visited(const int cell_id) const { return _visited[cell_id]; }
    bool visited(const Cell* cell) const { return _visited[cell->id]; }

    void clear() {
        _head = _tail = 0;
        std::fill(_visited.begin(), _visited.end(), 0);
    }

    bool add(const int cell_id) {
        if (_visited[cell_id]) return false;
        _queue[_tail++] = cell_id;
        _visited[cell_id] = 1;
        return true;
    }

    bool add(const Cell* cell) { return add(cell->id); }

    std::optional<const Cell*> next() {
        if (_head == _tail) return std::nullopt;
        return _level->cells[_queue[_head++]];
    }
};
