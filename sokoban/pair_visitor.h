#pragma once
//#include <iostream>

//#include "core/exception.h"
//#include "core/vector.h"
#include "core/array_deque.h"
#include "core/matrix.h"

class PairVisitor : public each<PairVisitor> {
   public:
    PairVisitor(uint size1, uint size2) { visited.resize(size1, size2); }

    bool try_add(uint a, uint b) {
        if (visited(a, b)) return false;
        visited(a, b) = true;
        deque.push_back({a, b});
        return true;
    }

    void reset() {
        deque.clear();
        visited.fill(false);
    }

    std::optional<std::pair<uint, uint>> next() {
        if (deque.empty()) return std::nullopt;
        ON_SCOPE_EXIT(deque.pop_front());
        return deque.front();
    }

   private:
    mt::array_deque<std::pair<uint, uint>> deque;
    matrix<bool> visited;
};
