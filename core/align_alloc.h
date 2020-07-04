#pragma once
#include <stdlib.h>

#include <new>

template <typename T, std::size_t N = sizeof(T)>
class AlignAlloc {
   public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    typedef T* pointer;
    typedef const T* const_pointer;

    typedef T& reference;
    typedef const T& const_reference;

    AlignAlloc() noexcept {}
    template <typename T2>
    AlignAlloc(const AlignAlloc<T2, N>&) noexcept {}
    ~AlignAlloc() noexcept {}

    pointer address(reference r) { return &r; }
    const_pointer address(const_reference r) const { return &r; }

    pointer allocate(size_type n) {
        size_t p = reinterpret_cast<size_t>(malloc(n * sizeof(value_type) + N + sizeof(void*)));
        if (p == 0) throw std::bad_alloc();
        size_t q = p + sizeof(void*);
        if (q % N) q += N - q % N;
        reinterpret_cast<size_t*>(q)[-1] = p;
        return reinterpret_cast<pointer>(q);
    }

    void deallocate(pointer p, size_type) {
        if (p) free(reinterpret_cast<void**>(p)[-1]);
    }

    void construct(pointer p, const value_type& wert) { new (p) value_type(wert); }

    void destroy(pointer p) { p->~value_type(); }

    size_type max_size() const noexcept { return size_type(-1) / sizeof(value_type); }

    template <typename T2>
    struct rebind {
        typedef AlignAlloc<T2, N> other;
    };

    bool operator!=(const AlignAlloc<T, N>& other) const { return !(*this == other); }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    // Always returns true for stateless allocators.
    bool operator==(const AlignAlloc<T, N>& other) const { return true; }
};
