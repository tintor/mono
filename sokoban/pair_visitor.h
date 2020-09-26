#pragma once
#include "core/array_deque.h"
#include "core/matrix.h"

class PairVisitor : public each<PairVisitor> {
   public:
    PairVisitor(ushort size1, ushort size2) { visited.resize(size1, size2); }

    bool try_add(ushort a, ushort b) {
        if (visited(a, b)) return false;
        visited(a, b) = true;
        deque.push_back({a, b});
        return true;
    }

    void reset() {
        deque.clear();
        visited.fill(false);
    }

    std::optional<std::pair<ushort, ushort>> next() {
        if (deque.empty()) return std::nullopt;
        ON_SCOPE_EXIT(deque.pop_front());
        return deque.front();
    }

   private:
    mt::array_deque<std::pair<ushort, ushort>> deque;
    matrix<char> visited;
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

    std::optional<Node> next() {
        if (_remaining.empty()) return std::nullopt;

        ON_SCOPE_EXIT(_remaining.pop_front());
        return _remaining.top();
    }
};
