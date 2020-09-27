#pragma once
#include "sokoban/pair_visitor.h"
#include "sokoban/level.h"

struct AgentBoxVisitor : public each<AgentBoxVisitor> {
    AgentBoxVisitor(const Level* level) : _level(level), _visitor(level->cells.size(), level->num_alive) {}

    bool add(const Cell* a, const Cell* b) {
        return _visitor.add(a->id, b->id);
    }

    void clear() {
        _visitor.clear();
    }

    optional<pair<const Cell*, const Cell*>> next() {
        auto n = _visitor.next();
        if (!n) return nullopt;
        return pair(_level->cells[n->first], _level->cells[n->second]);
    }

private:
    const Level* _level;
    PairVisitor _visitor;
};

// Queue API:
// - bool empty()
// - void push_back(const Node& node)
// - void pop_front()
// - const Node& top()
// - void clear()
// Set API:
// - bool operator[](const Node& node)
// - pair<?, bool> emplace(const Node& node) -> returns false in bool if Node already exists
// - void clear()
template<typename Queue, typename Set, typename Node>
class GraphVisitor : public each<GraphVisitor<Queue, Set, Node>> {
    Queue _remaining;
    Set _visited;

public:
    // TODO better name as this includes node that were just enqueued() and not visited through next() yet!
    bool visited(const Node& node) const { return _visited[node]; }

    void clear() {
        _remaining.clear();
        _visited.clear();
    }

    bool add(const Node& node) {
        if (!_visited.emplace(node).second) return false;

        _remaining.push_back(node);
        return false;
    }

    optional<Node> next() {
        if (_remaining.empty()) return nullopt;

        ON_SCOPE_EXIT(_remaining.pop_front());
        return _remaining.top();
    }
};
