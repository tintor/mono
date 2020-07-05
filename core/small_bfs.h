#pragma once
#include <vector>
#include <optional>

#include "core/each.h"
#include "core/numeric.h"

// very simple and fast array queue (but number of pushes is limited)
template <typename T>
class small_queue {
   public:
    small_queue(uint capacity) : _data(capacity) {}
    bool empty() const { return _head == _tail; }
    operator bool() const { return !empty(); }
    void push(T e) { _data[_tail++] = e; }
    T pop() { return _data[_head++]; }
    void clear() { _head = _tail = 0; }
    uint tail() const { return _tail; }

   private:
    std::vector<T> _data;
    uint _head = 0;
    uint _tail = 0;
};

template <typename T>
struct small_bfs : public each<small_bfs<T>> {
    small_queue<T> queue;
    std::vector<uchar> visited;  // avoid slower vector<bool> as it is bit compressed!

    small_bfs(uint capacity) : queue(capacity) { visited.resize(capacity, 0); }

    void clear() {
        queue.clear();
        for (auto& e : visited) e = 0;
    }

    void add(T e, uint index) {
        if (!visited[index]) {
            visited[index] = 1;
            queue.push(e);
        }
    }

    std::optional<T> next() {
        if (queue) return queue.pop();
        return std::nullopt;
    }
};
