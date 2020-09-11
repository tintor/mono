#pragma once
#include "sokoban/util.h"
#include "sokoban/level.h"
#include "sokoban/counters.h"
#include "sokoban/heuristic.h"
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

template <typename Boxes>
bool contains_box_blocked_goals(const Cell* agent, const Boxes& non_frozen, const Boxes& frozen) {
    const Level* level = agent->level;
    PairVisitor visitor(level->cells.size(), level->num_alive);

    for (Cell* g : level->goals()) {
        if (frozen[g->id]) continue;

        visitor.reset();
        // Uses "moves" as this is reverse search
        for (auto [_, e] : g->moves)
            if (!frozen[e->id]) visitor.try_add(e->id, g->id);

        bool goal_reachable = false;
        for (auto [a_id, b_id] : visitor) {
            if (a_id == agent->id && non_frozen[b_id]) {
                goal_reachable = true;
                break;
            }

            const Cell* a = level->cells[a_id];
            const Cell* b = level->cells[b_id];

            // Uses "moves" as this is reverse search
            for (auto [d, n] : a->moves) {
                if (frozen[n->id]) continue;
                if (n != b) visitor.try_add(n->id, b->id); // move
                if (a->dir(d ^ 2) && a->dir(d ^ 2) == b) visitor.try_add(n->id, a->id); // pull
            }
        }

        if (!goal_reachable) return true;
    }

    return false;
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
    bool matches(const int agent, const Boxes& boxes, bool print_it = false) {
        /*if (print_it) {
            static std::mutex m;
            std::unique_lock lock(m);
            print("matches?\n");
            Print(_level, TState<Boxes>(agent, boxes));
        }*/

        // TODO assume Boxes is always dense and avoid this conversion!
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
            if (!matches_pattern(agent, wboxes, p)) continue;
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

    std::string summary() const {
        vector<int> count(100);
        _mutex.lock_shared();
        const Word* end = _words.data() + _words.size();
        for (const Word* p = _words.data(); p < end; p += _agent_words + _box_words) {
            int b = 0;
            for (int i = 0; i < _box_words; i++) b += popcount(p[_agent_words + i]);
            count[b] += 1;
        }
        _mutex.unlock_shared();
        std::string s;
        for (int i = 0; i < count.size(); i++) if (count[i] > 0) s += format(" {}:{}", i, count[i]);
        return s;
    }

private:
    bool matches_pattern(const int agent, const Word* boxes, const Word* p) {
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
            for (const Cell* b : a->new_moves) {
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
    DeadlockDB(const Level* level) : _level(level), _patterns(level) {

    }

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

        Result result = TIMER(contains_frozen_boxes(_level->cells[agent], boxes, boxes_copy, num_boxes, q), q.contains_frozen_boxes_ticks);
        if (result == Result::Frozen) {
            std::unique_lock<mutex> lock(_add_mutex); // To prevent race condition of two threads adding the same pattern.
            if (!_patterns.matches(agent, boxes_copy)) {
                //print("pre-minimization:\n");
                //Print(_level, TState<Boxes>(agent, boxes));
                minimize_pattern(agent, boxes_copy, num_boxes);
                //print("post-minimization:\n");
                //Print(_level, TState<Boxes>(agent, boxes));

                if (!is_trivial_pattern(boxes_copy, num_boxes) && !solved(_level, boxes_copy)) {
                    _patterns.add(agent, boxes_copy);
                }
            }
        }
        if (result == Result::BlockedGoal) {
            // TODO add goal room deadlock pattern
        }

        if (result != Result::NotFrozen) {
            q.frozen_box_deadlocks += 1;
            return true;
        }
        return false;
    }

    size_t size() const { return _patterns.size(); }

    std::string summary() const { return _patterns.summary(); }

private:
    void load_patterns(const std::string_view filename) {
    }

    void save_pattern(const int agent, const Boxes& boxes) {
    }

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

    enum class Result {
        NotFrozen,
        Frozen,
        BlockedGoal,
    };

    Result contains_frozen_boxes_const(const Cell* agent, const Boxes& boxes, int num_boxes) {
        static thread_local Counters dummy_counters;
        Boxes mutable_boxes = boxes;
        return contains_frozen_boxes(agent, boxes, mutable_boxes, num_boxes, dummy_counters);
    }

    Result contains_frozen_boxes(const Cell* agent, const Boxes& orig_boxes, Boxes& boxes, int& num_boxes, Counters& q) {
        // Fast-path
        AgentVisitor visitor(agent);
        for (const Cell* a : visitor)
            for (auto [d, b] : a->actions) {
                if (!boxes[b->id]) {
                    // agent moves to B
                    visitor.add(b);
                    continue;
                }

                const Cell* c = b->dir(d);
                if (!c || !c->alive || boxes[c->id]) continue;

                boxes.reset(b->id);
                boxes.set(c->id);
                auto bb = b;
                auto dd = d;
                bool m = is_simple_deadlock(c, boxes) || (!TIMER(is_reversible_push(bb, boxes, dd), q.fb1a_ticks) && TIMER(_patterns.matches(a->id, boxes, true), q.fb1b_ticks));
                boxes.reset(c->id);
                if (m) {
                    boxes.set(b->id);
                    continue;
                }

                // agent pushes box from B to C (and box disappears)
                if (--num_boxes == 1) { q.fb1 += 1; return Result::NotFrozen; }
                visitor.add(b);
            }

        if (solved(_level, boxes) && all_empty_goals_are_reachable(_level, visitor, boxes)) { q.fb2 += 1; return Result::NotFrozen; }

        // Slow-path (clear visitor after each push)
        visitor.clear();
        visitor.add(agent);
        for (const Cell* a : visitor)
            for (auto [d, b] : a->actions) {
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
                if (--num_boxes == 1) { q.fb3 += 1; return Result::NotFrozen; }
                visitor.clear();  // <-- Necessary for edge cases.
                visitor.add(b);
                break; // <-- Necesary for edge cases.
            }

        if (!solved(agent->level, boxes)) { q.fb4 += 1; return Result::Frozen; }
        if (!all_empty_goals_are_reachable(_level, visitor, boxes)) { q.fb5 += 1; return Result::BlockedGoal; }
        if (contains_box_blocked_goals(agent, orig_boxes, boxes)) { q.fb6 += 1; return Result::BlockedGoal; }
        q.fb7 += 1;
        return Result::NotFrozen;
    }

private:
    const Level* _level;
    std::mutex _add_mutex;
    Patterns _patterns;
};
