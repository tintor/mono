#pragma once
#include "sokoban/util.h"
#include "sokoban/level.h"
#include "sokoban/counters.h"
#include <shared_mutex>

using std::vector;
using std::array;
using std::mutex;

template <typename Boxes>
bool solved(const Level* level, const Boxes& boxes) {
    for (int i = level->goals().size(); i < level->alive().size(); i++) {
        if (boxes[i]) return false;
    }
    return true;
}

template <typename Boxes>
bool all_empty_goals_are_reachable(const Level* level, AgentVisitor& visitor, const Boxes& boxes) {
    for (int i = 0; i < level->goals().size(); i++) {
        if (!visitor.visited(i) && !boxes[i]) return false;
    }
    return true;
}

// Thread-safe, and allows adding duplicate / redundant patterns!
class Patterns {
    using Word = uint;
    constexpr static int WordBits = sizeof(Word) * 8;
public:
    Patterns(const Level* level)
        : _level(level)
        , _num_alive(level->alive().size())
        , _agent_words((_level->cells.size() + WordBits - 1) / WordBits)
        , _box_words((_num_alive + WordBits - 1) / WordBits) {
        _words.reserve((_agent_words + _box_words) * 64);
    }

    template <typename Boxes>
    bool matches(const int agent, const Boxes& boxes) {
        array<Word, 100> boxes_static;
        vector<Word> boxes_dynamic;
        Word* wboxes;
        if (_num_alive <= boxes_static.size()) {
            wboxes = boxes_static.data();
        } else {
            boxes_dynamic.resize(_num_alive, 0);
            wboxes = boxes_dynamic.data();
        }

        std::fill(wboxes, wboxes + _box_words, 0);
        for (int i = 0; i < _num_alive; i++) {
            if (boxes[i]) add_bit(wboxes, i);
        }

        _mutex.lock_shared();
        const Word* end = _words.data() + _words.size();
        for (const Word* p = _words.data(); p < end; p += _agent_words + _box_words) {
            if (!matches(agent, wboxes, p)) continue;
            _mutex.unlock_shared();
            return true;
        }
        _mutex.unlock_shared();
        return false;
    }

    template <typename Boxes>
    void add(const int agent, const Boxes& boxes) {
        _mutex.lock();
        _words.resize(_words.size() + _agent_words + _box_words, 0);
        Word* p = _words.data() + _words.size() - _agent_words - _box_words;
        write_pattern(agent, boxes, p);

        /*Print(_level, TState(agent, boxes), [this, p](const Cell* e) {
            if (e->id < _num_alive && has_bit(p + _agent_words, e->id)) return "🔵";
            if (!has_bit(p, e->id)) return "▫️ ";
            return "  ";
        });*/

        _mutex.unlock();
    }

    size_t size() const {
        _mutex.lock_shared();
        size_t size = _words.size() / (_agent_words + _box_words);
        _mutex.unlock_shared();
        return size;
    }

private:
    bool matches(const int agent, const Word* boxes, const Word* p) {
        if (!has_bit(p, agent)) return false;

        const Word* pb = p + _agent_words;
        for (int i = 0; i < _box_words; i++) {
            if ((boxes[i] | pb[i]) != boxes[i]) return false;
        }
        return true;
    }

    template <typename Boxes>
    void write_pattern(const int agent, const Boxes& boxes, Word* p) {
        // Agent part
        AgentVisitor visitor(_level, agent);
        for (const Cell* a : visitor) {
            add_bit(p, a->id);
            for (auto [_, b] : a->moves) {
                if (!boxes[b->id]) visitor.add(b);
            }
        }

        // Box part
        for (int i = 0; i < _num_alive; i++) {
            if (boxes[i]) {
                add_bit(p + _agent_words, i);
            }
        }
    }

    static bool has_bit(const Word* p, int index) {
        const Word mask = Word(1) << (index % WordBits);
        return (p[index / WordBits] & mask) != 0;
    }

    static void add_bit(Word* p, int index) {
        p[index / WordBits] |= Word(1) << (index % WordBits);
    }

    const Level* _level;
    const int _num_alive;
    const int _agent_words;
    const int _box_words;

    mutable std::shared_mutex _mutex;
    vector<Word> _words;
};

template <typename Boxes>
class DeadlockDB {
    using Word = uint;
    constexpr static int WordBits = sizeof(Word) * 8;

public:
    DeadlockDB(const Level* level) : _level(level), _patterns(level) {}

    bool is_deadlock(const int agent, const Boxes& boxes, const Cell* pushed_box, const int push_dir, Counters& q) {
        Timestamp deadlock_ts;
        if (TIMER(is_simple_deadlock(pushed_box, boxes), q.is_simple_deadlock_ticks)) {
            q.simple_deadlocks += 1;
            return true;
        }

        if (TIMER(is_reversible_push(_level->cells[agent], boxes, push_dir), q.is_reversible_push_ticks)) {
            q.reversible_pushes += 1;
            return false;
        }

        if (TIMER(_patterns.matches(agent, boxes), q.db_contains_pattern_ticks)) {
            q.db_deadlocks += 1;
            return true;
        }

        Boxes boxes_copy = boxes;
        int num_boxes = _level->goals().size(); // TODO assumption!

        Result result = TIMER(contains_frozen_boxes(_level->cells[agent], boxes_copy, num_boxes), q.contains_frozen_boxes_ticks);
        if (result == Result::Frozen) {
            std::unique_lock<mutex> lock(_add_mutex); // To prevent race condition of two threads adding the same pattern.
            if (!_patterns.matches(agent, boxes_copy)) {
                //fmt::print("pre-minimization:\n");
                //Print(_level, TState<Boxes>(agent, boxes));
                minimize_pattern(agent, boxes_copy, num_boxes);
                //fmt::print("post-minimization:\n");
                //Print(_level, TState<Boxes>(agent, boxes));

                if (!is_trivial_pattern(boxes_copy, num_boxes) && !solved(_level, boxes_copy)) {
                    _patterns.add(agent, boxes_copy);
                }
            }
        }
        if (result == Result::UnreachableGoal) {
            // TODO add goal room deadlock pattern
        }

        if (result != Result::NotFrozen) {
            q.frozen_box_deadlocks += 1;
            return true;
        }
        return false;
    }

    size_t size() const { return _patterns.size(); }

private:
    bool is_trivial_pattern(const Boxes& boxes, const int num_boxes) {
        if (num_boxes > 3) return false;
        const int num_alive = _level->alive().size();
        for (int i = 0; i < num_alive; i++) {
            if (boxes[i]) return is_simple_deadlock(_level->cells[i], boxes);
        }
        return false;
    }

    void minimize_pattern(const int agent, Boxes& boxes, int& num_boxes) {
        if (num_boxes <= 2) return;

        const int num_alive = _level->alive().size();
        while (true) {
            bool reduced = false;
            for (int i = 0; i < num_alive; i++) {
                if (!boxes[i]) continue;

                boxes.reset(i);
                if (contains_frozen_boxes_const(_level->cells[agent], boxes, num_boxes) == Result::NotFrozen) {
                    boxes.set(i);
                    continue;
                }
                reduced = true;
                num_boxes -= 1;
                if (num_boxes <= 2) return;
            }
            if (!reduced) break;
        }
    }

    // Is it possible to push any box, without ending up in known deadlock state?
    bool contains_pushable_box(const Cell* agent, Boxes boxes) {
        AgentVisitor visitor(agent);
        for (const Cell* a : visitor)
            for (auto [d, b] : a->moves) {
                if (visitor.visited(b)) continue;
                if (!boxes[b->id]) {
                    // agent moves to B
                    visitor.add(b);
                    continue;
                }

                const Cell* c = b->dir(d);
                if (!c || !c->alive || boxes[c->id]) continue;

                boxes.reset(b->id);
                boxes.set(c->id);
                // TODO reversible check
                bool m = is_simple_deadlock(c, boxes) || _patterns.matches(a->id, boxes);
                boxes.reset(c->id);
                if (m) {
                    boxes.set(b->id);
                    continue;
                }

                return true;
            }
        return false;
    }

    enum class Result {
        NotFrozen,
        Frozen,
        UnreachableGoal
    };

    Result contains_frozen_boxes_const(const Cell* agent, Boxes boxes, int num_boxes) {
        return contains_frozen_boxes(agent, boxes, num_boxes);
    }

    Result contains_frozen_boxes(const Cell* agent, Boxes& boxes, int& num_boxes) {
        // Fast-path
        AgentVisitor visitor(agent);
        for (const Cell* a : visitor)
            for (auto [d, b] : a->moves) {
                if (visitor.visited(b)) continue; // TODO is this really needed?
                if (!boxes[b->id]) {
                    // agent moves to B
                    visitor.add(b);
                    continue;
                }

                const Cell* c = b->dir(d);
                if (!c || !c->alive || boxes[c->id]) continue;

                boxes.reset(b->id);
                boxes.set(c->id);
                bool m = is_simple_deadlock(c, boxes) || (!is_reversible_push(b, boxes, d) && _patterns.matches(a->id, boxes));
                boxes.reset(c->id);
                if (m) {
                    boxes.set(b->id);
                    continue;
                }

                // agent pushes box from B to C (and box disappears)
                if (--num_boxes == 1) return Result::NotFrozen;
                visitor.add(b);
            }

        if (solved(_level, boxes) && all_empty_goals_are_reachable(_level, visitor, boxes)) return Result::NotFrozen;

        // Slow-path (clear visitor after each push)
        visitor.clear();
        visitor.add(agent);
        for (const Cell* a : visitor)
            for (auto [d, b] : a->moves) {
                if (visitor.visited(b)) continue; // TODO is this really needed?
                if (!boxes[b->id]) {
                    // agent moves to B
                    visitor.add(b);
                    continue;
                }

                const Cell* c = b->dir(d);
                if (!c || !c->alive || boxes[c->id]) continue;

                boxes.reset(b->id);
                boxes.set(c->id);
                bool m = is_simple_deadlock(c, boxes) || (!is_reversible_push(b, boxes, d) && _patterns.matches(a->id, boxes));
                boxes.reset(c->id);
                if (m) {
                    boxes.set(b->id);
                    continue;
                }

                // agent pushes box from B to C (and box disappears)
                if (--num_boxes == 1) return Result::NotFrozen;
                visitor.clear();  // <-- Necessary for edge cases.
                visitor.add(b);
                break; // <-- Necesary for edge cases.
            }

        if (!solved(agent->level, boxes)) return Result::Frozen;
        if (!all_empty_goals_are_reachable(_level, visitor, boxes)) return Result::UnreachableGoal;
        // TODO additional check: unpushable goal
        // convert (agent, boxes) to LevelEnv AND run pull algorithm from every goal (to some box)
        return Result::NotFrozen;
    }

private:
    const Level* _level;
    std::mutex _add_mutex;
    Patterns _patterns;
};
