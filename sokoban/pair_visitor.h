#pragma once
#include "sokoban/common.h"

class PairVisitor : public each<PairVisitor> {
   public:
    PairVisitor(ushort size1, ushort size2) { visited.resize(size1, size2); }

    bool add(ushort a, ushort b) {
        if (visited(a, b)) return false;
        visited(a, b) = true;
        deque.push_back({a, b});
        return true;
    }

    void clear() {
        deque.clear();
        visited.fill(false);
    }

    optional<pair<ushort, ushort>> next() {
        if (deque.empty()) return nullopt;
        ON_SCOPE_EXIT(deque.pop_front());
        return deque.front();
    }

   private:
    array_deque<pair<ushort, ushort>> deque;
    matrix<char> visited;
};
