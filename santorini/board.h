#pragma once
#include <array>
#include <fstream>
#include <filesystem>
#include "absl/container/flat_hash_map.h"

#include "core/algorithm.h"
#include "core/numeric.h"
#include "core/column.h"
#include "core/tensor.h"
#include "core/fmt.h"

#include "santorini/cell.h"
#include "santorini/coord.h"

using Cells = std::array<Cell, 25>;
size_t CellsHash(const Cells& cells);

struct MiniBoard {
    Cells cell;

    size_t hash() const {
        if (hash_code == 0) hash_code = CellsHash(cell);
        return hash_code;
    }

    bool operator==(const MiniBoard& b) const { return cell == b.cell; }

private:
    mutable size_t hash_code = 0;
};

enum class Phase { PlaceWorker, MoveBuild, GameOver };
enum class Card { None, Apollo };

inline std::string_view CardName(Card c) {
    if (c == Card::None) return "none";
    if (c == Card::Apollo) return "Apollo";
    Check(false);
    return "";
}

struct Board : public MiniBoard {
    Phase phase = Phase::PlaceWorker;
    Figure player = Figure::Player1; // Will be set to winner in GameOver phase.
    Card card1 = Card::None;
    Card card2 = Card::None;

    Card my_card() const { return (player == Figure::Player1) ? card1 : card2; }
    Card opp_card() const { return (player == Figure::Player1) ? card2 : card1; }

    // Inner action state
    std::optional<Coord> moved;
    bool built = false;

    const Cell& operator()(Coord c) const { return cell[c.v]; }
    Cell& operator()(Coord c) { return cell[c.v]; }

    bool operator==(const Board& b) const {
        return phase == b.phase && player == b.player && moved == b.moved && built == b.built && cell == b.cell;
    }
};

inline Figure Winner(const Board& board) {
    return (board.phase != Phase::GameOver) ? Figure::None : board.player;
}

inline std::ostream& operator<<(std::ostream& os, const MiniBoard& board) {
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            const Cell c = board.cell[row * 5 + col];
            os << char(c.figure) << char(c.level ? '0' + c.level : '.') << ' ';
        }
        os << std::endl;
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Board& board) {
    os << static_cast<MiniBoard>(board);
    char p[2] = {char(board.player), 0};
    os << format("phase {}, player {}", board.phase, p);
    if (board.moved) os << format(" moved {}{}", board.moved->x(), board.moved->y());
    os << format(" built {}", board.built) << std::endl;
    return os;
}

inline void Print(const Board& board) { *fos << board; }

inline void Print(const MiniBoard& board) { *fos << board; }

inline void Render(const MiniBoard& board) {
    const std::string color[4] = { "\033[0m", "\033[0;34m", "\033[0;33m", "\033[1;31m" };
    const char* tower = ".#xo";

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            const auto c = board.cell[row * 5 + col];
            // Color
            *fos << color[int(c.level)];
            // Figure
            if (c.figure == Figure::Dome) *fos << '*';
            else if (c.figure == Figure::Player1) *fos << 'A';
            else if (c.figure == Figure::Player2) *fos << 'B';
            else if (c.figure == Figure::None) *fos << tower[int(c.level)];
            else *fos << char(c.figure);
            *fos << ' ';
        }
        // Reset color
        *fos << "\033[0m";
        *fos << std::endl;
    }
}

inline bool Less(const Cells& a, const Cells& b) {
    for (int i = 0; i < 25; i++) {
        if (a[i] != b[i]) return Less(a[i], b[i]);
    }
    return false;
}

inline Cells Transform(const Cells& cell, int transform) {
    Cells out;
    for (Coord e : kAll) out[e.v] = cell[Transform(e, transform).v];
    return out;
}

// Returns the same board for all symmetry variations!
inline MiniBoard Normalize(const MiniBoard& board) {
    // generate all 8 transforms and return the smallest one
    MiniBoard out = board;
    for (int transform = 1; transform < 8; transform++) {
        Cells m = Transform(board.cell, transform);
        if (Less(m, out.cell)) out.cell = m;
    }
    return out;
}

inline bool IsEmpty(const MiniBoard& board) {
    for (const Cell& c : board.cell) if (c.figure != Figure::None || c.level != 0) return false;
    return true;
}

namespace std {
template <>
struct hash<Board> {
    size_t operator()(const Board& b) const { return b.hash(); }
};
}  // namespace std

namespace std {
template <>
struct hash<MiniBoard> {
    size_t operator()(const MiniBoard& b) const { return b.hash(); }
};
}  // namespace std

template <typename Fn>
int Count(const Board& board, const Fn& fn) {
    return CountIf(kAll, L(fn(board(e))));
}

// Network input description:
// Board (5x5xCell):
// Cell:
// 4 - level
// 1 - dome
// 1 - ego
// 1 - opponent

constexpr int CellBits = 4 + 3;
constexpr int BoardBits = 25 * CellBits;

inline void ToTensor(const MiniBoard& board, tensor out) {
    if (out.shape() != dim4(5, 5, 7)) Fail(out.shape().str());
    FOR(x, 5) FOR(y, 5) {
        tensor::type* s = out.data() + out.offset(x, y);
        auto cell = board.cell[y * 5 + x];
        s[0] = cell.level == 0;
        s[1] = cell.level == 1;
        s[2] = cell.level == 2;
        s[3] = cell.level == 3;
        s[4] = cell.figure == Figure::Dome;
        s[5] = cell.figure == Figure::Player1;
        s[6] = cell.figure == Figure::Player2;
    }
}

inline MiniBoard FromTensor(tensor out) {
    if (out.shape() != dim4(5, 5, 7)) Fail(out.shape().str());
    MiniBoard board;
    FOR(x, 5) FOR(y, 5) {
        tensor::type* s = out.data() + out.offset(x, y);
        auto& cell = board.cell[y * 5 + x];
        if (s[0]) cell.level = 0;
        if (s[1]) cell.level = 1;
        if (s[2]) cell.level = 2;
        if (s[3]) cell.level = 3;
        if (s[4]) cell.figure = Figure::Dome;
        if (s[5]) cell.figure = Figure::Player1;
        if (s[6]) cell.figure = Figure::Player2;
    }
    return board;
}

struct Score {
    uint p1 = 0, p2 = 0;
    Score() {}
    Score(const Score& s) : p1(s.p1), p2(s.p2) {}
    Score(uint p1, uint p2) : p1(p1), p2(p2) { }
    Score(Figure w) : p1((w == Figure::Player1) ? 1 : 0), p2((w == Figure::Player2) ? 1 : 0) {}

    float ValueP1() const { return p1 / float(p1 + p2); }
    Score operator+(Score o) { return {p1 + o.p1, p2 + o.p2}; }
    void operator+=(Score o) { *this = *this + o; }
};

// TODO move to core/sharded_flat_hash_map.h
template<typename Key, typename Value, size_t Shards>
class sharded_flat_hash_map {
public:
    size_t size() const { return Sum<size_t>(m_shard, [](const auto& e) { std::unique_lock lock(e._mutex); return e.data.size(); }); }

    void clear() {
        for (auto& e : m_shard) {
            std::unique_lock lock(e._mutex);
            e.data.clear();
        }
    }

    void increment(const Key& key, const Value& zero, const Value& value) {
        Shard& shard = m_shard[std::hash<Key>()(key) % Shards];
        std::unique_lock lock(shard._mutex);
        shard.data.emplace(key, zero).first->second += value;
    }

    void merge(const sharded_flat_hash_map& o) {
        FOR(i, Shards) {
            Shard& shard = m_shard[i];
            const Shard& o_shard = o.m_shard[i];

            std::unique_lock lock(shard._mutex);
            std::unique_lock lock2(o_shard._mutex);

            for (const auto& e : o_shard.data) {
                shard.data.emplace(e.first, Score()).first->second += e.second;
            }
        }
    }

    std::optional<Value> lookup(const Key& key) const {
        const Shard& shard = m_shard[std::hash<Key>()(key) % Shards];
        std::unique_lock lock(shard._mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return std::nullopt;
        return it->second;
    }

    template<typename Visitor>
    void each_locked(const Visitor& visitor) {
        for (auto& shard : m_shard) {
            std::unique_lock lock(shard._mutex);
            for (const auto& e : shard.data) visitor(e);
        }
    }

private:
    struct Shard {
        absl::flat_hash_map<Key, Value> data;
        mutable std::mutex _mutex;
    } m_shard[Shards];
};

class Values {
    static constexpr int Shards = 64;
   public:
    Values() {}

    Values(std::filesystem::path filename) {
        vtensor out({5, 5, 7});
        std::ifstream is(filename);
        while (is) {
            FOR(x, 5) FOR(y, 5) {
                tensor::type* s = out.data() + out.offset(x, y);
                char b;
                if (!is.get(b)) return;
                FOR(i, 7) s[i] = (b & (1 << i)) ? 1 : 0;
            }
            Score score;
            if (!is.read(reinterpret_cast<char*>(&score.p1), sizeof(score.p1))) return;
            if (!is.read(reinterpret_cast<char*>(&score.p2), sizeof(score.p2))) return;
            m_data.increment(FromTensor(out), Score(), score);
        }
    }

    size_t Size() const { return m_data.size(); }

    /*const auto& Sample(std::mt19937_64& random) const {
        Check(m_data.size() > 0);
        auto it = (m_data.bucket_count() != m_last_buckets) ? m_data.begin() : m_last_iterator;
        std::uniform_int_distribution<int> dis(0, 9);
        for (auto i : range(dis(random))) {
            if (++it == m_data.end()) it = m_data.begin();
        }
        m_last_buckets = m_data.bucket_count();
        m_last_iterator = it;
        return *it;
    }*/

    void Clear() { m_data.clear(); }

    void Add(const MiniBoard& board, Score score) {
        m_data.increment(Normalize(board), Score(), score);
    }

    void Merge(const Values& values) { m_data.merge(values.m_data); }

    std::optional<Score> Lookup(const MiniBoard& board) const {
        return m_data.lookup(Normalize(board));
    }

    float ValueP1(const MiniBoard& board) const {
        auto score = Lookup(board);
        return score.has_value() ? score->ValueP1() : 0.5f;
    }

    void Export(std::filesystem::path filename) {
        vtensor out({5, 5, 7});
        std::ofstream os(filename);
        m_data.each_locked([&](const std::pair<MiniBoard, Score>& e) {
            auto [board, score] = e;
            ToTensor(board, out);
            FOR(x, 5) FOR(y, 5) {
                tensor::type* s = out.data() + out.offset(x, y);
                char b = 0;
                FOR(i, 7) if (s[i] > 0) b |= 1 << i;
                os.put(b);
            }
            os.write(reinterpret_cast<const char*>(&score.p1), sizeof(score.p1));
            os.write(reinterpret_cast<const char*>(&score.p2), sizeof(score.p2));
        });
    }

   private:
    sharded_flat_hash_map<MiniBoard, Score, 64> m_data;
};
