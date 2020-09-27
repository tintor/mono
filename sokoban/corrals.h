#pragma once
#include <vector>
#include "sokoban/frozen.h"
#include "sokoban/level.h"
#include "sokoban/util.h"

// corral = unreachable area surrounded by boxes which are either:
// - frozen on goal OR (reachable by agent and only pushable inside)

// if corral contains a goal (assuming num_boxes == num_goals) or one of its fence boxes isn't on goal
// then that corral must be prioritized for push (ignoring all other corrals)

constexpr int kCapacity = 2000;
using Corral = static_vector<uchar, kCapacity>;  // avoid slower bit optimized vector<bool>

bool is_single_component(const Level* level, const Corral& corral);

inline void add(Corral& dest, const Corral& src) {
    for (size_t i = 0; i < min(dest.size(), src.size()); i++)
        if (src[i]) dest[i] = true;
}

template <typename State>
class Corrals {
   public:
    using Boxes = typename State::Boxes;

    Corrals(const Level* level) : _level(level), _corral(level->cells.size()), _reachable(level->cells.size()) {}
    void find_unsolved_picorral(const State& s);

    bool has_picorral() const { return _has_picorral; }
    const Corral& picorral() const { return _picorral; }
    std::optional<Corral> opt_picorral() const {
        if (_has_picorral) return _picorral;
        return std::nullopt;
    }

   private:
    void find_corrals(const State& s);
    void add_if_picorral(const Boxes& boxes);

    const Level* _level;
    Corral _corral;
    std::vector<std::pair<Corral, bool>> _corrals;
    int _corrals_size;
    std::vector<uchar> _reachable;

    bool _has_picorral;
    int _picorral_pushes;
    Corral _picorral;
};

template <typename State>
void PrintWithCorral(const Level* level, const State& s, const std::optional<Corral>& corral) {
    Print(level, s.agent, s.boxes, [&](Cell* c) {
        if (!corral.has_value() || !(*corral)[c->id]) return "";
        if (s.boxes[c->id]) return c->goal ? "🔷" : "⚪";
        if (c->goal) return "❔";
        if (!c->alive) return "❕";
        return "▫️ ";
    });
}

// corral with a goal without a box OR a box not on goal
template <typename Boxes>
bool is_unsolved_corral(const Level* level, const Boxes& boxes, const Corral& corral) {
    for (Cell* a : level->alive())
        if (corral[a->id] && a->goal != boxes[a->id]) return true;
    return false;
}

// I-Corral is a corral where all boxes on the barrier can only be pushed inwards (into the corral) by the first push
// PI-Corral is an I-Corral where the player can perform all legal first pushes into the corral,
// meaning the player can reach all the relevant boxes from all relevant directions.
template <typename Boxes>
bool is_picorral(const Level* level, const Boxes& boxes, const std::vector<uchar>& reachable, const Corral& corral,
                 int& count) {
    for (Cell* a : level->alive())
        if (corral[a->id] && boxes[a->id])
            for (auto [b, q] : a->pushes) {
                if (!corral[b->id] && !corral[q->id]) return false;
                if (!boxes[b->id] && corral[b->id] && !corral[q->id]) {
                    count += 1;
                    if (boxes[q->id]) {
                        if (is_frozen_on_goal_simple(q, boxes)) continue;
                        return false;
                    }
                    Boxes nboxes(boxes);
                    nboxes.reset(a->id);
                    nboxes.set(b->id);
                    if (is_simple_deadlock(b, nboxes)) continue;
                    if (!reachable[q->id]) return false;
                }
            }
    return true;
}

template <typename State>
void Corrals<State>::find_corrals(const State& s) {
    AgentVisitor visitor(_level, s.agent);
    for (auto& e : _reachable) e = false;
    for (const Cell* a : visitor) {
        _reachable[a->id] = true;
        for (auto [_, b] : a->moves)
            if (!s.boxes[b->id])
                visitor.add(b);
            else
                _reachable[b->id] = true;
    }

    _corrals.clear();
    for (Cell* q : _level->cells)
        if (!s.boxes[q->id] && !visitor.visited(q)) {
            if (_level->cells.size() > kCapacity) THROW(runtime_error, "level has more cells ({}) than capacity ({})", _level->cells.size(), kCapacity);
            static_vector<uchar, kCapacity> corral(_level->cells.size(), false);

            visitor.add(q);
            for (const Cell* a : visitor) {
                corral[a->id] = true;

                for (const Cell* b : a->dir8)
                    if (b && !corral[b->id] && s.boxes[b->id]) corral[b->id] = true;

                for (auto [_, b] : a->moves)
                    if (!s.boxes[b->id])
                        visitor.add(b);
                    else
                        corral[b->id] = true;
            }
            bool unsolved = is_unsolved_corral(_level, s.boxes, corral);
            _corrals.emplace_back(std::move(corral), unsolved);
        }
}

template <typename State>
void Corrals<State>::add_if_picorral(const Boxes& boxes) {
    int pushes = 0;
    if (is_picorral(_level, boxes, _reachable, _corral, /*out*/ pushes))
        if (!_has_picorral || pushes < _picorral_pushes) {
            _picorral = _corral;
            _picorral_pushes = pushes;
            _has_picorral = true;
        }
}

// optimize: find the most expensive cases and improve them
template <typename State>
void Corrals<State>::find_unsolved_picorral(const State& s) {
    find_corrals(s);

    _has_picorral = false;
    _picorral_pushes = std::numeric_limits<int>::max();
    if (_corrals.size() >= 8) {
        // size 1
        for (const auto& c : _corrals)
            if (c.second) {
                _corral = c.first;
                add_if_picorral(s.boxes);
            }
        // size 2
        FOR(a, _corrals.size())
            for (size_t b = a + 1; b < _corrals.size(); b++)
                if (_corrals[a].second || _corrals[b].second) {
                    // generate subset from individual corrals
                    _corral = _corrals[a].first;
                    add(_corral, _corrals[b].first);
                    add_if_picorral(s.boxes);
                }
        // size all
        for (auto& e : _corral) e = false;
        FOR(i, _corrals.size()) add(_corral, _corrals[i].first);
        if (is_unsolved_corral(_level, s.boxes, _corral)) add_if_picorral(s.boxes);
    } else {
        for (size_t subset = 1; subset < (1lu << _corrals.size()); subset++) {
            // subset must contain at least one unsolved corral
            bool unsolved = false;
            FOR(i, _corrals.size()) {
                size_t m = 1lu << i;
                if ((subset & m) == m && _corrals[i].second) {
                    unsolved = true;
                    break;
                }
            }
            if (!unsolved) continue;

            for (auto& e : _corral) e = false;
            FOR(i, _corrals.size()) {
                size_t m = 1lu << i;
                if ((subset & m) == m) add(_corral, _corrals[i].first);
            }
            add_if_picorral(s.boxes);
        }
    }
}
