#pragma once
#include <initializer_list>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <algorithm>

template<typename T>
class raw_array {
   public:
    raw_array() : _data(nullptr) {}

    raw_array(size_t size, bool zero_init = false) {
        _data = reinterpret_cast<T*>(zero_init ? calloc(sizeof(T), size) : malloc(sizeof(T) * size));
        if (_data == nullptr) throw std::bad_alloc();
    }

    raw_array(const raw_array&) = delete;

    raw_array(raw_array&& e) {
        _data = e._data;
        e._data = nullptr;
    }

    ~raw_array() { free(_data); }

    raw_array& operator=(raw_array&& e) {
        if (this != &e) {
            free(_data);
            _data = e._data;
            e._data = nullptr;
        }
        return *this;
    }

    raw_array& operator=(const raw_array& e) = delete;

    T& operator[](size_t index) { return _data[index]; }

    const T& operator[](size_t index) const { return _data[index]; }

    void resize(size_t size) {
        if (size == 0) {
            free(_data);
            _data = nullptr;
            return;
        }
        T* data = reinterpret_cast<T*>(realloc(_data, sizeof(T) * size));
        if (!data) throw std::bad_alloc();
        _data = data;
    }

    // TODO potentially faster as it can avoid copying
    void resize_no_copy(size_t size) {
        resize(size);
    }

    const T* data() const { return _data; }
    T* data() { return _data; }

    void swap(raw_array& o) { std::swap(_data, o._data); }

   private:
    T* _data;
};

inline unsigned int round_up_power2(unsigned int a) {
    a -= 1;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a += 1;
    return a;
}

inline unsigned long round_up_power2(unsigned long a) {
    a -= 1;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a |= a >> 32;
    a += 1;
    return a;
}

template<typename size_type, size_type InitialCapacity = 4>
struct default_growth {
    size_type operator()(size_type capacity) const {
        return std::max<size_type>(InitialCapacity, round_up_power2(capacity));
    }
};

// TODO std::hash
// TODO use std::allocator
template <typename T,
          typename Allocator = std::allocator<T>,
          typename Size = unsigned int,
          typename growth = default_growth<Size>>
class array_deque {
   public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = Size;
    using reference = T&;
    using const_reference = const T&;
    //using pointer =
    //using const_pointer =

    array_deque() noexcept(noexcept(Allocator())) {}

    explicit array_deque(const Allocator& alloc) noexcept {}

    explicit array_deque(size_type size) : m_data(size), m_capacity(size), m_size(size) {
        for (size_type i = 0; i < m_size; i++) new (&m_data[i]) T();
    }

    array_deque(size_type size, const T& init, const Allocator& alloc = Allocator()) : m_data(size), m_capacity(size), m_size(size) {
        for (size_type i = 0; i < m_size; i++) new (&m_data[i]) T(init);
    }

    // TODO test
    template <class InputIt>
    array_deque(InputIt first, InputIt last, const Allocator& alloc = Allocator()) : m_data(std::distance(first, last)), m_capacity(std::distance(first, last)), m_size(std::distance(first, last)) {
        size_type i = 0;
        while (first != last) new (&m_data[i++]) T(*first++);
    }

    array_deque(const array_deque& o) : m_data(o.size()), m_capacity(o.size()), m_size(o.size()) {
        for (size_type i = 0; i < m_size; i++) new (&m_data[i]) T(o[i]);
    }

    array_deque(const array_deque& o, const Allocator& alloc) : m_data(o.size()), m_capacity(o.size()), m_size(o.size()) {
        for (size_type i = 0; i < m_size; i++) new (&m_data[i]) T(o[i]);
    }

    array_deque(array_deque&& o) noexcept : m_data(std::move(o.m_data)), m_capacity(o.m_capacity), m_start(o.m_start), m_size(o.m_size) {
        o.m_capacity = 0;
        o.m_start = 0;
        o.m_size = 0;
    }

    // TODO test
    array_deque(std::initializer_list<T> init, const Allocator& alloc = Allocator()) : m_data(init.size()), m_capacity(size), m_size(size) {
        size_type i = 0;
        for (const T& e : init) new (&m_data[i++]) T(e);
    }

    // TODO test for destroying two elements
    ~array_deque() { clear(); }

    array_deque& operator=(const array_deque& o) {
        if (this == &o) return *this;

        clear_resize(o.size());
        for (size_type i = 0; i < m_size; i++) new (&m_data[i]) T(o[i]);
        return *this;
    }

    array_deque& operator=(array_deque&& o)
            noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
                || std::allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &o) return *this;

        m_data = std::move(o.m_data);
        m_capacity = o.m_capacity;
        m_start = o.m_start;
        m_size = o.m_size;

        o.m_capacity = 0;
        o.m_start = 0;
        o.m_size = 0;
        return *this;
    }

    // TODO test
    array_deque& operator=(std::initializer_list<T> ilist) {
        clear_resize(ilist.size());
        size_type i = 0;
        for (const T& e : ilist) new (&m_data[i++]) T(e);
        return *this;
    }

    // TODO private
    void clear_resize(size_type count) {
        clear();
        if (count > capacity()) {
            m_capacity = growth()(count);
            m_data.resize_no_copy(m_capacity);
        }
        m_start = 0;
        m_size = count;
    }

    void assign(size_type count, const T& value) {
        clear_resize(count);
        for (size_type i = 0; i < count; i++) new (&m_data[i++]) T(value);
    }

    template <class InputIt>
    void assign(InputIt first, InputIt last) {
        clear_resize(std::distance(first, last));
        size_type i = 0;
        while (first != last) new (&m_data[i++]) T(*first++);
    }

    void assign(std::initializer_list<T> ilist) { operator=(ilist); }

    constexpr allocator_type get_allocator() const noexcept;

    // TODO test
    constexpr reference at(size_type pos) {
        if (!(pos < m_size)) throw std::out_of_range("");
        return m_data[offset(pos)];
    }

    constexpr const_reference at(size_type pos) const {
        if (!(pos < m_size)) throw std::out_of_range("");
        return m_data[offset(pos)];
    }

    constexpr T& operator[](size_type pos) { return m_data[offset(pos)]; }

    constexpr const T& operator[](size_type pos) const { return m_data[offset(pos)]; }

    constexpr T& front() { return m_data[m_start]; }

    constexpr const T& front() const { return m_data[m_start]; }

    constexpr T& back() { return m_data[offset(m_size - 1)]; }

    constexpr const T& back() const { return m_data[offset(m_size - 1)]; }

    // TODO operator--
    struct iterator {
        array_deque& deque;
        T* ptr;

        iterator(array_deque& d, T* p) : deque(d), ptr(p) {}

        T& operator*() { return *ptr; }
        iterator operator++() {
            ptr += 1;
            if (ptr == deque.data_end()) ptr = deque.data_begin();
            return *this;
        }
        iterator operator++(int) {
            auto it = *this;
            ptr += 1;
            if (ptr == deque.data_end()) ptr = deque.data_begin();
            return it;
        }
        bool operator==(const iterator& o) const { return ptr == o.ptr; }
        bool operator!=(const iterator& o) const { return ptr != o.ptr; }
    };

    struct const_iterator {
        const array_deque& deque;
        const T* ptr;

        const_iterator(const array_deque& d, const T* p) : deque(d), ptr(p) {}

        const T& operator*() const { return *ptr; }
        const_iterator operator++() {
            ptr += 1;
            if (ptr == deque.data_end()) ptr = deque.data_begin();
            return *this;
        }
        const_iterator operator++(int) {
            auto it = *this;
            ptr += 1;
            if (ptr == deque.data_end()) ptr = deque.data_begin();
            return it;
        }
        bool operator==(const const_iterator& o) const { return ptr == o.ptr; }
        bool operator!=(const const_iterator& o) const { return ptr != o.ptr; }
    };

    constexpr iterator begin() noexcept { return iterator(*this, &m_data[m_start]); }

    constexpr iterator end() noexcept { return iterator(*this, &m_data[offset(m_size)]); }

    // const_iterator begin() noexcept const { return cbegin(); }

    constexpr const_iterator cbegin() const noexcept { return const_iterator(*this, &m_data[m_start]); }

    // constexpr const_iterator end() noexcept const { return cend(); }

    constexpr const_iterator cend() const noexcept { return const_iterator(*this, &m_data[offset(m_size)]); }

    // TODO reverse_iterator
    // TODO const_reverse_iterator

    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

    constexpr size_type size() const noexcept { return m_size; }

    constexpr size_type max_size() const noexcept { return std::numeric_limits<size_type>::max(); }

    constexpr void reserve(size_type new_capacity) {
        if (new_capacity <= capacity()) return;

        // resize in place if possible
        if (m_start + m_size <= capacity()) {
            m_data.resize(new_capacity);
            m_capacity = new_capacity;
            return;
        }

        raw_array<T> new_data(new_capacity);
        for (size_type i = 0; i < m_size; i++) {
            T& e = m_data[offset(i)];
            new (&new_data[i]) T(std::move(e));
            e.~T();
        }
        m_data = std::move(new_data);
        m_capacity = new_capacity;
        m_start = 0;
    }

    constexpr size_type capacity() const noexcept { return m_capacity; }

    void shrink_to_fit() {
        if (m_size == capacity()) return;

        if (m_start == 0) {
            m_data.resize(m_size);
            m_capacity = m_size;
            return;
        }

        raw_array<T> new_data(m_size);
        for (size_type i = 0; i < m_size; i++) {
            T& e = m_data[offset(i)];
            new (&new_data[i]) T(std::move(e));
            e.~T();
        }
        m_data = std::move(new_data);
        m_capacity = m_size;
    }

    void clear() noexcept {
        for (size_type i = 0; i < m_size; i++) m_data[offset(i)].~T();
        m_size = 0;
    }

    iterator insert(const_iterator pos, const T& value);

    iterator insert(const_iterator pos, T&& value);

    iterator insert(const_iterator pos, size_type count, const T& value);

    template <class InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last);

    iterator insert(const_iterator pos, std::initializer_list<T> ilist);

    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args);

    iterator erase(const_iterator pos);

    iterator erase(const_iterator first, const_iterator last);

    void push_front(const T& value) {
        ensure_space();
        size_type i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (&m_data[i]) T(value);
        m_size += 1;
        m_start = i;
    }

    void push_front(T&& value) {
        ensure_space();
        size_type i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (&m_data[i]) T(std::move(value));
        m_size += 1;
        m_start = i;
    }

    template<class... Args>
    reference emplace_front(Args&&... args) {
        ensure_space();
        size_type i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (&m_data[i]) T(args...);
        m_size += 1;
        m_start = i;
    }

    void pop_front() {
        size_type i = m_start++;
        if (m_start >= capacity()) m_start -= capacity();
        m_size -= 1;
        m_data[i].~T();
    }

    void push_back(const T& value) {
        ensure_space();
        size_type i = offset(m_size);
        new (&m_data[i]) T(value);
        m_size += 1;
    }

    void push_back(T&& value) {
        ensure_space();
        size_type i = offset(m_size);
        new (&m_data[i]) T(std::move(value));
        m_size += 1;
    }

    template<class... Args>
    reference emplace_back(Args&&... args) {
        ensure_space();
        size_type i = offset(m_size);
        new (&m_data[i]) T(args...);
        m_size += 1;
    }

    void pop_back() {
        size_type i = offset(m_size--);
        m_data[i].~T();
    }

    void resize(size_type count);

    void resize(size_type count, const value_type& value);

    constexpr void swap(array_deque& o)
            noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value
            || std::allocator_traits<Allocator>::is_always_equal::value) {
        m_data.swap(o.m_data);
        std::swap(m_capacity, o.m_capacity);
        std::swap(m_start, o.m_start);
        std::swap(m_size, o.m_size);
    }

    constexpr bool operator==(const array_deque& o) const {
        if (m_size != o.size()) return false;
        for (size_type i = 0; i < m_size; i++) if (!(operator[](i) == o.operator[](i))) return false;
        return true;
    }

    constexpr bool operator!=(const array_deque& o) const { return !operator==(o); }

    constexpr bool operator<(const array_deque& o) const;

    constexpr bool operator<=(const array_deque& o) const;

    constexpr bool operator>(const array_deque& o) const;

    constexpr bool operator>=(const array_deque& o) const;

   private:
    const T* data_begin() const { return m_data.data(); }
    const T* data_end() const { return m_data.data() + capacity(); }
    T* data_begin() { return m_data.data(); }
    T* data_end() { return m_data.data() + capacity(); }

    size_type offset(Size index) const {
        size_type o = m_start + index;
        if (o >= capacity()) o -= capacity();
        return o;
    }

    void ensure_space() {
        if (m_size == capacity()) reserve(growth()(capacity() + 1));
    }

   private:
    raw_array<T> m_data;
    Size m_capacity = 0;
    Size m_start = 0;
    Size m_size = 0;
};

// Deduction guide
template<class InputIt, class Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
array_deque(InputIt, InputIt, Alloc = Alloc()) -> array_deque<typename std::iterator_traits<InputIt>::value_type, Alloc>;

namespace std {

template <class T, class Alloc>
constexpr void swap(array_deque<T, Alloc>& a, array_deque<T, Alloc>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

template <class T, class Alloc, class U>
constexpr typename array_deque<T, Alloc>::size_type erase(array_deque<T, Alloc>& c, const U& value) {
    auto it = std::remove(c.begin(), c.end(), value);
    auto r = std::distance(it, c.end());
    c.erase(it, c.end());
    return r;
}

template <class T, class Alloc, class Pred>
constexpr typename array_deque<T, Alloc>::size_type erase_if(array_deque<T, Alloc>& c, Pred pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    auto r = std::distance(it, c.end());
    c.erase(it, c.end());
    return r;
}

}
