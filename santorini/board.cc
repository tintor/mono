#include <array>
#include <random>

#include "core/numeric.h"
#include "santorini/cell.h"

using Cells = std::array<Cell, 25>;

static std::array<size_t, 7> zorbist[25];

static struct InitZorbist {
    InitZorbist() {
        std::mt19937_64 re(0);
        FOR(i, 25) for (auto& e : zorbist[i]) e = re();
    }
} init_zorbist;

size_t CellsHash(const Cells& cells) {
    size_t h = 0;
    FOR(i, 25) {
        const auto& z = zorbist[i];
        auto c = cells[i];
        h ^= z[c.level];
        if (c.figure == Figure::Dome) h ^= z[4];
        if (c.figure == Figure::Player1) h ^= z[5];
        if (c.figure == Figure::Player2) h ^= z[6];
    }
    return h ? h : 1;
}
