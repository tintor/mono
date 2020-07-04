#pragma once
#include <cassert>

#include "core/numeric.h"

inline ulong rotl64(ulong x, char r) { return (x << r) | (x >> (64 - r)); }

inline ulong getblock64(const ulong* p, int i) { return p[i]; }

inline ulong fmix64(ulong k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdllu;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53llu;
    k ^= k >> 33;
    return k;
}

#define ROTL64(x, y) rotl64(x, y)

inline size_t MurmurHash3_x64_128(const void* key, const int len, const uint seed) {
    const uchar* data = (const uchar*)key;
    const int nblocks = len / 16;

    ulong h1 = seed;
    ulong h2 = seed;

    const ulong c1 = 0x87c37b91114253d5llu;
    const ulong c2 = 0x4cf5ad432745937fllu;

    //----------
    // body

    const ulong* blocks = (const ulong*)(data);

    for (int i = 0; i < nblocks; i++) {
        ulong k1 = getblock64(blocks, i * 2 + 0);
        ulong k2 = getblock64(blocks, i * 2 + 1);

        k1 *= c1;
        k1 = ROTL64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = ROTL64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = ROTL64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = ROTL64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    //----------
    // tail

    const uchar* tail = (const uchar*)(data + nblocks * 16);

    ulong k1 = 0;
    ulong k2 = 0;

    switch (len & 15) {
        case 15:
            k2 ^= ((ulong)tail[14]) << 48;
        case 14:
            k2 ^= ((ulong)tail[13]) << 40;
        case 13:
            k2 ^= ((ulong)tail[12]) << 32;
        case 12:
            k2 ^= ((ulong)tail[11]) << 24;
        case 11:
            k2 ^= ((ulong)tail[10]) << 16;
        case 10:
            k2 ^= ((ulong)tail[9]) << 8;
        case 9:
            k2 ^= ((ulong)tail[8]) << 0;
            k2 *= c2;
            k2 = ROTL64(k2, 33);
            k2 *= c1;
            h2 ^= k2;

        case 8:
            k1 ^= ((ulong)tail[7]) << 56;
        case 7:
            k1 ^= ((ulong)tail[6]) << 48;
        case 6:
            k1 ^= ((ulong)tail[5]) << 40;
        case 5:
            k1 ^= ((ulong)tail[4]) << 32;
        case 4:
            k1 ^= ((ulong)tail[3]) << 24;
        case 3:
            k1 ^= ((ulong)tail[2]) << 16;
        case 2:
            k1 ^= ((ulong)tail[1]) << 8;
        case 1:
            k1 ^= ((ulong)tail[0]) << 0;
            k1 *= c1;
            k1 = ROTL64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    return h1 ^ h2;
}

// corrent order of calls:
// 1) reset
// 2) append (0 or more times)
// 3) tail[_slow]
// 4) finalize >> 128bit hash will be stored in h1 and h2
class Murmur3_x64_128 {
   private:
    void mix(ulong k1, ulong k2) {
        const ulong c1 = 0x87c37b91114253d5llu;
        const ulong c2 = 0x4cf5ad432745937fllu;
        k1 *= c1;
        k1 = rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;
        k2 *= c2;
        k2 = rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;
    }

    static ulong rotl64(ulong x, char r) { return (x << r) | (x >> (64 - r)); }

    static ulong fmix64(ulong k) {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdllu;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53llu;
        k ^= k >> 33;
        return k;
    }

   public:
    void reset(uint seed) {
        h1 = seed;
        h2 = seed;
    }

    void append(ulong k1, ulong k2) {
        mix(k1, k2);
        h1 = rotl64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;
        h2 = rotl64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    // len of the entire string!
    void tail_slow(const uchar* tail, int len) {
        assert((len & 15) != 0);
        const ulong c1 = 0x87c37b91114253d5llu;
        const ulong c2 = 0x4cf5ad432745937fllu;

        ulong k1 = 0, k2 = 0;
        switch (len & 15) {
            case 15:
                k2 ^= ((ulong)tail[14]) << 48;
            case 14:
                k2 ^= ((ulong)tail[13]) << 40;
            case 13:
                k2 ^= ((ulong)tail[12]) << 32;
            case 12:
                k2 ^= ((ulong)tail[11]) << 24;
            case 11:
                k2 ^= ((ulong)tail[10]) << 16;
            case 10:
                k2 ^= ((ulong)tail[9]) << 8;
            case 9:
                k2 ^= ((ulong)tail[8]) << 0;
                k2 *= c2;
                k2 = rotl64(k2, 33);
                k2 *= c1;
                h2 ^= k2;
            case 8:
                k1 ^= ((ulong)tail[7]) << 56;
            case 7:
                k1 ^= ((ulong)tail[6]) << 48;
            case 6:
                k1 ^= ((ulong)tail[5]) << 40;
            case 5:
                k1 ^= ((ulong)tail[4]) << 32;
            case 4:
                k1 ^= ((ulong)tail[3]) << 24;
            case 3:
                k1 ^= ((ulong)tail[2]) << 16;
            case 2:
                k1 ^= ((ulong)tail[1]) << 8;
            case 1:
                k1 ^= ((ulong)tail[0]) << 0;
                k1 *= c1;
                k1 = rotl64(k1, 31);
                k1 *= c2;
                h1 ^= k1;
        };
    }

    void tail(ulong k1, ulong k2) { mix(k1, k2); }

    void finalize(int len) {
        h1 ^= len;
        h2 ^= len;

        h1 += h2;
        h2 += h1;

        h1 = fmix64(h1);
        h2 = fmix64(h2);

        h1 += h2;
        h2 += h1;
    }

    ulong h1, h2;
};
