#pragma once
#include <type_traits>
#include <vector>

#include "core/numeric.h"

template<typename T, class Enable = void>
class array_deque; // undefined if T is not trivial type!

template <typename T>
class array_deque<T, std::enable_if_t<std::is_trivial_v<T>>> {
   public:
    array_deque() {}
    array_deque(uint capacity) : m_data(capacity) {}

    array_deque(const array_deque& o) {
        m_data.resize(o.capacity());
        m_start = 0;
        m_size = o.size();
        for (uint i = 0; i < m_size; i++) {
            operator[](i) = o[i];
        }
    }

    auto operator=(const array_deque& o) {
        if (this == &o) {
            return *this;
        }
        if (o.size() > capacity()) {
            m_data.resize(round_up_power2(o.size()));
        }
        m_start = 0;
        m_size = o.size();
        for (uint i = 0; i < m_size; i++) {
            operator[](i) = o[i];
        }
        return *this;
    }

    uint size() const { return m_size; }
    uint capacity() const { return m_data.size(); }
    bool empty() const { return m_size == 0; }

    const T& operator[](uint index) const {
        return (m_start + index < capacity()) ? m_data[m_start + index] : m_data[m_start + index - capacity()];
    }
    T& operator[](uint index) {
        return (m_start + index < capacity()) ? m_data[m_start + index] : m_data[m_start + index - capacity()];
    }

    void push_back(T value) {
        if (m_size == m_data.size()) {
            if (m_data.size() == 0) {
                m_data.resize(4);
            } else {
                std::vector<T> data(m_data.size() * 2);
                for (uint i = 0; i < m_size; i++) std::swap(data[i], operator[](i));
                m_data.swap(data);
            }
            m_start = 0;
        }
        uint end = m_start + m_size;
        if (end >= capacity()) {
            end -= capacity();
        }
        m_data[end] = value;
        m_size += 1;
    }

    T pop_front() {
        uint i = m_start++;
        if (m_start >= capacity()) {
            m_start -= capacity();
        }
        m_size -= 1;
        return m_data[i];
    }

    void swap(array_deque& o) {
        m_data.swap(o.m_data);
        std::swap(m_start, o.m_start);
        std::swap(m_size, o.m_size);
    }

    void clear() {
        m_start = 0;
        m_size = 0;
    }

   private:
    std::vector<T> m_data;
    uint m_start = 0;
    uint m_size = 0;
};
