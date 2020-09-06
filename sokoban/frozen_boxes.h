#pragma once
#include "sokoban/util.h"
#include "sokoban/level.h"
#include <shared_mutex>
using std::vector;
using std::array;
using std::mutex;

class DeadlockDB {
    using Word = uint;
    constexpr static int WordBits = sizeof(Word) * 8;

public:
    DeadlockDB(const Level* level)
        : _level(level)
        , _num_alive(level->alive().size())
        , _agent_words((_level->cells.size() + WordBits - 1) / WordBits)
        , _box_words((_num_alive + WordBits - 1) / WordBits) {
        _words.reserve((_agent_words + _box_words) * 64);
    }

    template <typename State>
    bool ContainsPattern(const State& s) {
        array<Word, 100> boxes_static;
        vector<Word> boxes_dynamic;
        Word* boxes;
        if (_num_alive <= boxes_static.size()) {
            boxes = boxes_static.data();
        } else {
            boxes_dynamic.resize(_num_alive, 0);
            boxes = boxes_dynamic.data();
        }

        std::fill(boxes, boxes + _box_words, 0);
        for (int i = 0; i < _num_alive; i++) {
            if (s.boxes[i]) AddBit(boxes, i);
        }

        _mutex.lock_shared();
        const Word* end = _words.data() + _words.size();
        for (const Word* p = _words.data(); p < end; p += _agent_words + _box_words) {
            if (!MatchesPattern(s.agent, boxes, p)) continue;
            _mutex.unlock_shared();
            return true;
        }
        _mutex.unlock_shared();
        return false;
    }

    template <typename State>
    void AddPattern(const State& s) {
        std::unique_lock<mutex> lock(_add_mutex); // To prevent race condition of two threads adding the same pattern.
        if (ContainsPattern(s)) return;

        _mutex.lock();
        _words.resize(_words.size() + _agent_words + _box_words, 0);
        WritePattern(s, _words.data() + _words.size() - _agent_words - _box_words);
        _mutex.unlock();

        if (!ContainsPattern(s)) THROW(runtime_error, "pattern not stored");
    }

private:
    bool MatchesPattern(const int agent, const Word* boxes, const Word* p) {
        if (!HasBit(p, agent)) return false;

        const Word* pb = p + _agent_words;
        for (int i = 0; i < _box_words; i++) {
            if ((boxes[i] | pb[i]) != boxes[i]) return false;
        }
        return true;
    }

    template <typename State>
    void WritePattern(const State& s, Word* p) {
        // Agent part
        small_bfs<const Cell*> visitor(_level->cells.size());
        visitor.add(_level->cells[s.agent], s.agent);
        for (const Cell* a : visitor) {
            AddBit(p, a->id);
            for (auto [_, b] : a->moves) {
                if (!s.boxes[b->id]) visitor.add(b, b->id);
            }
        }

        // Box part
        for (int i = 0; i < _num_alive; i++) {
            if (s.boxes[i]) {
                AddBit(p + _agent_words, i);
            }
        }

        /*Print(_level, s, [this, p](const Cell* e) {
            if (e->id < _num_alive && HasBit(p + _agent_words, e->id)) return "🔵";
            if (!HasBit(p, e->id)) return "▫️ ";
            return "  ";
        });*/
    }

    static bool HasBit(const Word* p, int index) {
        const Word mask = Word(1) << (index % WordBits);
        return (p[index / WordBits] & mask) != 0;
    }

    static void AddBit(Word* p, int index) {
        p[index / WordBits] |= Word(1) << (index % WordBits);
    }

    const Level* _level;
    const int _num_alive;
    const int _agent_words;
    const int _box_words;

    std::mutex _add_mutex;
    std::shared_mutex _mutex;
    vector<Word> _words;
};

// frozen box deadlock
template <typename Boxes>
bool contains_frozen_boxes(const Cell* agent, Boxes boxes, const int num_goals, const int num_alive,
                           small_bfs<const Cell*>& visitor, DeadlockDB& deadlock_db) {
    // Fast-path
    int num_boxes = num_goals;
    visitor.clear();
    visitor.add(agent, agent->id);

    for (const Cell* a : visitor)
        for (auto [d, b] : a->moves) {
            if (visitor.visited[b->id]) continue;
            if (!boxes[b->id]) {
                // agent moves to B
                visitor.add(b, b->id);
                continue;
            }

            const Cell* c = b->dir(d);
            if (!c || !c->alive || boxes[c->id]) continue;

            boxes.reset(b->id);
            boxes.set(c->id);
            bool m = is_simple_deadlock(c, boxes);
            boxes.reset(c->id);
            if (m) {
                boxes.set(b->id);
                continue;
            }

            // agent pushes box from B to C (and box disappears)
            if (--num_boxes == 1) return false;
            visitor.add(b, b->id);
        }

    // No frozen boxes if all remaining boxes are on goals
    bool hit = false;
    for (uint i = num_goals; i < num_alive; i++)
        if (boxes[i]) { hit = true; break; }
    if (!hit) return false;

    // Slow-path
    visitor.clear();
    visitor.add(agent, agent->id);

    for (const Cell* a : visitor)
        for (auto [d, b] : a->moves) {
            if (visitor.visited[b->id]) continue;
            if (!boxes[b->id]) {
                // agent moves to B
                visitor.add(b, b->id);
                continue;
            }

            const Cell* c = b->dir(d);
            if (!c || !c->alive || boxes[c->id]) continue;

            boxes.reset(b->id);
            boxes.set(c->id);
            bool m = is_simple_deadlock(c, boxes);
            boxes.reset(c->id);
            if (m) {
                boxes.set(b->id);
                continue;
            }

            // agent pushes box from B to C (and box disappears)
            if (--num_boxes == 1) return false;
            visitor.clear();  // <-- Necessary for edge cases.
            visitor.add(b, b->id);
            break; // <-- Necesary for edge cases.
        }

    // deadlock if any remaining box isn't on goal
    for (uint i = num_goals; i < num_alive; i++) {
        if (boxes[i]) {
            deadlock_db.AddPattern(TState<Boxes>(agent->id, boxes));
            return true;
        }
    }
    // deadlock if any remaining goal isn't reachable
    for (uint i = 0; i < num_goals; i++) {
        if (!visitor.visited[i] && !boxes[i]) return true;
    }
    return false;
}
