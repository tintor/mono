#pragma once
// no explicit size, but you can query to highest bit set (behaves like an big
// uint) expands to store new 1s, but doesn't shrink to store 0s has
// shrink_to_fit()
//
// has inline capacity for 64 bits

#include "core/bits_util.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/murmur3.h"

template <typename T>
T div_up(T a, T b) {
    return (a + b - 1) / b;
}

inline int hex_to_dec(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    throw std::invalid_argument("c");
}

// TODO switch internals to vector<Word>
class Bits {
   public:
    using Word = ulong;
    using Capacity = uint;
    static constexpr size_t WordBytes = sizeof(Word);
    static constexpr size_t WordBits = WordBytes * 8;

    // TESTED
    Bits() {
        _words = nullptr;
        _capacity = 0;
    }

    // TESTED
    explicit Bits(uint e) {
        if (e == 0) {
            _capacity = 0;
            _words = nullptr;
        } else {
            static_assert(WordBits >= 32);
            _capacity = 1;
            _words = alloc(1);
            _words[0] = e;
        }
    }

    // TESTED
    explicit Bits(ulong e) {
        if (e == 0) {
            _capacity = 0;
            _words = nullptr;
        } else {
            static_assert(WordBits >= 64);
            _capacity = 1;
            _words = alloc(1);
            _words[0] = e;
        }
    }

    // TESTED
    explicit Bits(__uint128_t e) {
        if (e == 0) {
            _capacity = 0;
            _words = nullptr;
        } else {
            static_assert(WordBits == 64);
            if (e <= std::numeric_limits<ulong>::max()) {
                _capacity = 1;
                _words = alloc(1);
                _words[0] = e;
            } else {
                _capacity = 2;
                _words = alloc(2);
                _words[0] = e;
                _words[1] = e >> 64;
            }
        }
    }

    // TESTED
    explicit Bits(std::string_view hex) {
        while (hex.size() > 0 && hex[0] == '0') hex = hex.substr(1);
        _capacity = div_up((hex.size() + 1) / 2, WordBytes);
        _words = alloc_zero(_capacity);
        const int CharsPerWord = 2 * WordBytes;
        for (size_t i = 0; i < hex.size(); i++) {
            size_t j = hex.size() - 1 - i;
            int shift = (j % CharsPerWord) * 4;
            _words[j / CharsPerWord] |= hex_to_dec(hex[i]) << shift;
        }
    }

    // TESTED
    Bits(const Bits& e) {
        _capacity = e.size_words();
        _words = alloc(_capacity);
        copy(e._words, _words, _capacity);
    }

    Bits(Bits&& e) {
        _words = e._words;
        _capacity = e._capacity;
        e._words = nullptr;
        e._capacity = 0;
    }

    ~Bits() { std::free(_words); }

    Bits& operator=(const Bits& e) {
        if (this != &e) {
            Capacity capacity = e.size_words();
            if (capacity > _capacity) {
                Word* words = alloc(capacity);
                std::free(_words);
                _words = words;
                copy(e._words, _words, capacity);
                _capacity = capacity;
            } else {
                copy(e.data(), _words, capacity);
                reset(_words + capacity, _capacity - capacity);
            }
        }
        return *this;
    }

    Bits& operator=(Bits&& e) {
        if (this != &e) {
            std::free(_words);
            _words = e._words;
            _capacity = e._capacity;
            e._words = nullptr;
            e._capacity = 0;
        }
        return *this;
    }

    Bits& operator=(ulong e) {
        if (e == 0) {
            reset(_words, _capacity);
        } else {
            if (_capacity == 0) {
                assert(_words == nullptr);
                _words = alloc(1);
                _capacity = 1;
            }
            _words[0] = e;
            reset(_words + 1, _capacity - 1);
        }
        return *this;
    }

    Bits& operator=(__uint128_t e) {
        if (e == 0) {
            reset(_words, _capacity);
        } else {
            static_assert(WordBits == 64);
            if (e <= std::numeric_limits<ulong>::max()) {
                if (_capacity < 1) {
                    assert(_words == nullptr);
                    _words = alloc(1);
                    _capacity = 1;
                }
                _words[0] = e;
                reset(_words + 1, _capacity - 1);
            } else {
                if (_capacity < 2) {
                    Word* words = alloc(2);
                    std::free(words);
                    _capacity = 2;
                }
                _words[0] = e;
                _words[1] = e >> 64;
                reset(_words + 2, _capacity - 2);
            }
        }
        return *this;
    }

    Bits& operator=(std::string_view hex) {
        while (hex.size() > 0 && hex[0] == '0') hex = hex.substr(1);
        Capacity capacity = div_up((hex.size() + 1) / 2, WordBytes);
        if (capacity > _capacity) {
            Word* words = alloc_zero(capacity);
            std::free(_words);
            _words = words;
            _capacity = capacity;
        } else {
            for (Capacity i = 0; i < _capacity; i++)
                if (_words[i] != 0) _words[i] = 0;
        }
        const int CharsPerWord = 2 * WordBytes;
        for (size_t i = 0; i < hex.size(); i++) {
            size_t j = hex.size() - 1 - i;
            int shift = (j % CharsPerWord) * 4;
            _words[j / CharsPerWord] |= hex_to_dec(hex[j]) << shift;
        }
        return *this;
    }

    // TODO method to copy range from bits A to bits B -> use it for substr

    Bits substr(size_t pos, size_t size) {
        // TODO
        Bits b;
        return b;
    }

    bool operator[](size_t index) const {
        if (index / WordBits >= _capacity) return false;
        Word mask = 1;
        mask <<= index % WordBits;
        return (_words[index / WordBits] & mask) != 0;
    }

    // TESTED
    void set(size_t index) {
        if (index + 1 >= _capacity * WordBits) reserve(round_up_power2(index + 1));
        Word mask = 1;
        mask <<= index % WordBits;
        _words[index / WordBits] |= mask;
    }

    void set(size_t index, bool value) {
        if (value)
            set(index);
        else
            reset(index);
    }

    // TODO set range of bits
    // TODO reset range of bits
    // TODO flip range of bits

    // RESET
    void reset(size_t index) {
        if (index / WordBits >= _capacity) return;
        Word mask = 1;
        mask <<= index % WordBits;
        _words[index / WordBits] &= ~mask;
    }

    // TESTED
    Word get_word(Capacity index) const { return index >= _capacity ? 0 : _words[index]; }

    // TESTED
    void set_word(Capacity index, Word word) {
        if (index >= _capacity) {
            if (word == 0) return;
            reserve_words(round_up_power2(index + 1));
        }
        _words[index] = word;
    }

    void flip(size_t index) {
        if (index >= _capacity * WordBits) reserve(round_up_power2(index + 1));
        Word mask = 1;
        mask <<= index % WordBits;
        _words[index / WordBits] ^= mask;
    }

    void compact() {
        // TODO release any unnecessary memory
    }

    void insert(size_t pos, const Bits& e) {
        // TODO insert / remove a range of bits
    }

    void remove_bit(size_t pos) {
        size_t x = pos / WordBits;
        if (x >= _capacity) return;
        Word mask = (static_cast<Word>(1) << x) - 1;
        _words[x] = (_words[x] & mask) | ((_words[x] >> 1) & ~mask) | (get_word(x + 1) << (WordBits - 1));
        for (Capacity i = x + 1; i < _capacity; i++) _words[i] = (_words[i] >> 1) | (get_word(i + 1) << (WordBits - 1));
    }

    void remove(size_t pos, size_t count) {
        // TODO faster
        for (size_t i = 0; i < count; i++) remove_bit(pos);
    }

    // TODO reserve should never shrink!
    void reserve_words(Capacity capacity) {
        if (capacity > _capacity) {
            realloc(capacity);
            reset(_words + _capacity, capacity - _capacity);
            _capacity = capacity;
            return;
        }
        if (capacity == _capacity) return;
        Capacity min_capacity = size_words();
        if (min_capacity == _capacity) return;
        if (capacity < min_capacity) capacity = min_capacity;
        realloc(capacity);
        _capacity = capacity;
    }

    // TESTED
    size_t popcount() const {
        size_t s = 0;
        for (Capacity i = 0; i < _capacity; i++) s += ::popcount(_words[i]);
        return s;
    }

    void reserve(size_t bits) { reserve_words(div_up(bits, WordBits)); }

    // TESTED
    Capacity size_words() const {
        for (Capacity i = _capacity - 1; i != static_cast<Capacity>(-1); i--)
            if (_words[i] != 0) return i + 1;
        return 0;
    }

    // TESTED
    size_t size() const {
        for (Capacity i = _capacity - 1; i != static_cast<Capacity>(-1); i--)
            if (_words[i] != 0) return i * WordBits + WordBits - clz(_words[i]);
        return 0;
    }

#define BITOP(OP)                                                                   \
    Capacity aw = a.size_words();                                                   \
    Capacity bw = b.size_words();                                                   \
    if (aw > bw) {                                                                  \
        c.reserve_words(aw);                                                        \
        for (Capacity i = 0; i < bw; i++) c._words[i] = a._words[i] OP b._words[i]; \
        if (&a != &c) {                                                             \
            for (Capacity i = bw; i < aw; i++) c._words[i] = a._words[i];           \
            for (Capacity i = aw; i < c.capacity(); i++) c._words[i] = 0;           \
        }                                                                           \
    } else {                                                                        \
        c.reserve_words(bw);                                                        \
        for (Capacity i = 0; i < aw; i++) c._words[i] = a._words[i] OP b._words[i]; \
        if (&b != &c) {                                                             \
            for (Capacity i = aw; i < bw; i++) c._words[i] = b._words[i];           \
            for (Capacity i = bw; i < c.capacity(); i++) c._words[i] = 0;           \
        }                                                                           \
    }

    // TODO allow arbitrary left shifts for A and B
    static void _and(const Bits& a, const Bits& b, Bits& c) { BITOP(&); }
    static void _or(const Bits& a, const Bits& b, Bits& c) { BITOP(|); }
    static void _xor(const Bits& a, const Bits& b, Bits& c) { BITOP(^); }

    static bool contains(const Bits& a, const Bits& b) {
        for (Capacity i = 0; i < std::min(a.capacity(), b.capacity()); i++) {
            auto aw = a.data()[i];
            auto bw = b.data()[i];
            if ((aw | bw) != aw) return false;
        }
        for (Capacity i = a.capacity(); i < b.capacity(); i++)
            if (b.data()[i]) return false;
        return true;
    }

    bool contains(const Bits& b) const { return Bits::contains(*this, b); }

    /*Bits opeator&(const Bits& b) const {
            Bits c;
            _and(*this, b, c);
            return c;
    }

    void operator &= (const Bits& b) { _and(*this, b, *this); }*/

    static void _not(const Bits& a, size_t size, Bits& c) {
        size = std::max(size, a.size());
        c.reserve_words(div_up(size, WordBits));
        Capacity aw = a.size_words();
        for (Capacity i = 0; i < aw; i++) c._words[i] = ~a._words[i];
        // TODO set bits from a.size() to size
        // TODO reset bits from size to _capacity * 8
    }
    static void _shl(const Bits& a, size_t shift, Bits& c) {
        // TODO
    }
    static void _shr(const Bits& a, size_t shift, Bits& c) {
        // TODO
    }

    const Word* data() const { return _words; }

    Word* data() { return _words; }

    Capacity capacity() const { return _capacity; }

    void clear() { reset(_words, _capacity); }

    bool empty() const { return is_zero(_words, _capacity); }

    bool operator==(const Bits& e) const {
        if (_capacity > e._capacity)
            return std::memcmp(_words, e._words, e._capacity) == 0 &&
                   is_zero(_words + e._capacity, _capacity - e._capacity);
        return std::memcmp(e._words, _words, _capacity) == 0 && is_zero(e._words + _capacity, e._capacity - _capacity);
    }

    bool operator!=(const Bits& e) const { return !operator==(e); }

    bool operator<(const Bits& e) const { return compare(*this, e) < 0; }
    bool operator>(const Bits& e) const { return compare(*this, e) > 0; }
    bool operator<=(const Bits& e) const { return compare(*this, e) <= 0; }
    bool operator>=(const Bits& e) const { return compare(*this, e) >= 0; }

    static int compare(const Bits& a, const Bits& b) {
        if (a.capacity() < b.capacity()) return -compare(b, a);

        if (!is_zero(a.data() + b.capacity(), a.capacity() - b.capacity())) return 1;

        for (Capacity w = b.capacity() - 1; w != static_cast<Capacity>(-1); w--) {
            if (a.data()[w] < b.data()[w]) return -1;
            if (a.data()[w] > b.data()[w]) return 1;
        }
        return 0;
    }

    void to_hex(std::string& hex) const {
        size_t digits = div_up<size_t>(size(), 16);
        hex.resize(digits);
        const int CharsPerWord = 2 * WordBytes;
        for (size_t i = 0; i < digits; i++) {
            int s = (i % CharsPerWord) * 8;
            Word d = (_words[i / CharsPerWord] >> s) & 0xF;
            hex[digits - 1 - i] = (d < 10) ? ('0' + d) : ('a' - 10 + d);
        }
    }

    std::string to_hex() const {
        std::string hex;
        to_hex(hex);
        return hex;
    }

    // TESTED
    size_t hash() const { return MurmurHash3_x64_128(_words, size_words() * WordBytes, 0); }

   private:
    static bool is_zero(const Word* src, Capacity capacity) {
        for (Capacity i = 0; i < capacity; i++)
            if (src[i] != 0) return false;
        return true;
    }

    static void copy(const Word* src, Word* dest, Capacity size) { std::memcpy(dest, src, WordBytes * size); }

    static void reset(Word* dest, Capacity size) { std::memset(dest, 0, WordBytes * size); }

    void realloc(Capacity capacity) {
        if (capacity == 0) {
            std::free(_words);
            _words = nullptr;
            return;
        }
        Word* words = reinterpret_cast<Word*>(std::realloc(_words, WordBytes * capacity));
        if (words == nullptr) throw std::bad_alloc();
        _words = words;
    }

    static Word* alloc(Capacity capacity) {
        if (capacity == 0) return nullptr;
        Word* words = reinterpret_cast<Word*>(std::malloc(WordBytes * capacity));
        if (words == nullptr) throw std::bad_alloc();
        return words;
    }

    static Word* alloc_zero(Capacity capacity) {
        if (capacity == 0) return nullptr;
        Word* words = reinterpret_cast<Word*>(std::calloc(capacity, WordBytes));
        if (words == nullptr) throw std::bad_alloc();
        return words;
    }

   private:
    Word* _words;
    Capacity _capacity;
};

namespace std {
template <>
struct hash<Bits> {
    size_t operator()(const Bits& a) const { return a.hash(); }
};
}  // namespace std
