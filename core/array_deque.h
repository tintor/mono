#pragma once
#include <cstdlib>
#include <new>

#include "core/bits_util.h"
#include "core/numeric.h"

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

// TODO std::hash

template <typename T, typename Size = uint, Size InitialCapacity = 4>
class array_deque {
   public:
    array_deque() {}

    array_deque(Size size) : m_data(size), m_capacity(size), m_size(size) {
        FOR(i, m_size) new (&m_data[i]) T();
    }

    array_deque(Size size, const T& init) : m_data(size), m_capacity(size), m_size(size) {
        FOR(i, m_size) new (&m_data[i]) T(init);
    }

    array_deque(const array_deque& o) : m_data(o.size()), m_capacity(o.size()), m_size(o.size()) {
        FOR(i, m_size) new (&m_data[i]) T(o[i]);
    }

    array_deque(array_deque&& o) : m_data(std::move(o.m_data)), m_capacity(o.m_capacity), m_start(o.m_start), m_size(o.m_size) {
        o.m_capacity = 0;
        o.m_start = 0;
        o.m_size = 0;
    }

    ~array_deque() {
        FOR(i, m_size) m_data[offset(0)].~T();
    }

    auto operator=(const array_deque& o) {
        if (this == &o) return *this;

        // destroy all elements
        FOR(i, m_size) m_data[offset(i)].~T();

        if (o.size() > capacity()) {
            m_data.resize_no_copy(o.size());
            m_capacity = o.size();
        }
        m_start = 0;
        m_size = o.size();

        FOR(i, m_size) new (&m_data[i]) T(o[i]);
        return *this;
    }

    auto operator=(array_deque&& o) {
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

    Size size() const { return m_size; }
    Size capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

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
            ON_SCOPE_EXIT({
                ptr += 1;
                if (ptr == deque.data_end()) ptr = deque.data_begin();
            });
            return *this;
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
            ON_SCOPE_EXIT({
                ptr += 1;
                if (ptr == deque.data_end()) ptr = deque.data_begin();
            });
            return *this;
        }
        bool operator==(const const_iterator& o) const { return ptr == o.ptr; }
        bool operator!=(const const_iterator& o) const { return ptr != o.ptr; }
    };

    iterator begin() { return iterator(*this, &m_data[m_start]); }
    iterator end() { return iterator(*this, &m_data[offset(m_size)]); }

    const_iterator begin() const { return const_iterator(*this, &m_data[m_start]); }
    const_iterator end() const { return const_iterator(*this, &m_data[offset(m_size)]); }

    const T& operator[](Size index) const { return m_data[offset(index)]; }
    T& operator[](Size index) { return m_data[offset(index)]; }

    const T& front() const { return m_data[m_start]; }
    T& front() { return m_data[m_start]; }

    const T& back() const { return m_data[offset(m_size - 1)]; }
    T& back() { return m_data[offset(m_size - 1)]; }

    void push_back(T&& value) {
        ensure_space();
        Size i = offset(m_size);
        new (&m_data[i]) T(std::move(value));
        m_size += 1;
    }

    void push_back(const T& value) {
        ensure_space();
        Size i = offset(m_size);
        new (&m_data[i]) T(value);
        m_size += 1;
    }

    T pop_back() {
        Size i = offset(m_size - 1);
        m_size -= 1;
        ON_SCOPE_EXIT(m_data[i].~T());
        return std::move(m_data[i]);
    }

    void push_front(T&& value) {
        ensure_space();
        Size i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (&m_data[i]) T(std::move(value));
        m_size += 1;
        m_start = i;
    }

    void push_front(const T& value) {
        ensure_space();
        Size i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (&m_data[i]) T(value);
        m_size += 1;
        m_start = i;
    }

    T pop_front() {
        Size i = m_start++;
        if (m_start >= capacity()) m_start -= capacity();
        m_size -= 1;
        ON_SCOPE_EXIT(m_data[i].~T());
        return std::move(m_data[i]);
    }

    void swap(array_deque& o) {
        m_data.swap(o.m_data);
        std::swap(m_capacity = o.m_capacity);
        std::swap(m_start, o.m_start);
        std::swap(m_size, o.m_size);
    }

    void reserve(Size new_capacity) {
        if (new_capacity <= capacity()) return;

        // resize in place if possible
        if (m_start + m_size <= capacity()) {
            m_data.resize(new_capacity);
            m_capacity = new_capacity;
            return;
        }

        raw_array<T> new_data(new_capacity);
        FOR(i, m_size) {
            T& e = m_data[offset(i)];
            new (&new_data[i]) T(std::move(e));
            e.~T();
        }
        m_data = std::move(new_data);
        m_capacity = new_capacity;
        m_start = 0;
    }

    void clear() {
        m_start = 0;
        m_size = 0;
    }

    void shrink_to_fit() {
        if (m_size == capacity()) return;

        if (m_start == 0) {
            m_data.resize(m_size);
            m_capacity = m_size;
            return;
        }

        raw_array<T> new_data(m_size);
        FOR(i, m_size) {
            T& e = m_data[offset(i)];
            new (&new_data[i]) T(std::move(e));
            e.~T();
        }
        m_data = std::move(new_data);
        m_capacity = m_size;
    }

    bool operator==(const array_deque& o) const {
        if (m_size != o.size()) return false;
        FOR(i, m_size) if (!(operator[](i) == o.operator[](i))) return false;
        return true;
    }

    bool operator!=(const array_deque& o) const { return !operator==(o); }

   private:
    const T* data_begin() const { return m_data.data(); }
    const T* data_end() const { return m_data.data() + capacity(); }
    T* data_begin() { return m_data.data(); }
    T* data_end() { return m_data.data() + capacity(); }

    Size offset(Size index) const {
        Size o = m_start + index;
        if (o >= capacity()) o -= capacity();
        return o;
    }

    void ensure_space() {
        if (m_size == capacity()) reserve(std::max<Size>(InitialCapacity, round_up_power2(capacity() + 1)));
    }

   private:
    raw_array<T> m_data;
    Size m_capacity = 0;
    Size m_start = 0;
    Size m_size = 0;
};
