#pragma once
#include <algorithm>
#include <iostream>
extern std::string ops;

namespace mt {

namespace detail {

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

template<class InputIt, class ForwardIt>
ForwardIt uninitialized_copy_const(InputIt first, InputIt last, ForwardIt d_first) {
    for (; first != last; ++d_first, (void) ++first)
       ::new (const_cast<void*>(static_cast<const void*>(std::addressof(*d_first))))
          typename std::iterator_traits<ForwardIt>::value_type(*first);
    return d_first;
}

template<class InputIt, class ForwardIt>
ForwardIt uninitialized_move_backward(InputIt first, InputIt last, ForwardIt d_last) {
    typedef typename std::iterator_traits<ForwardIt>::value_type Value;
    ForwardIt current = d_last;
    try {
        while (first != last) {
            ::new (static_cast<void*>(std::addressof(*--current))) Value(std::move(*--last));
        }
        return current;
    } catch (...) {
        while (d_last != current) (--d_last)->~Value();
        throw;
    }
}

template <typename ValueType, typename ArrayDeque>
struct array_deque_iterator {
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using pointer = ValueType*;
    using reference = ValueType&;
    using iterator_category = std::random_access_iterator_tag;

    using iterator = array_deque_iterator;
    ArrayDeque* deque = nullptr;
    size_t index = 0;

    array_deque_iterator(ArrayDeque* d, size_t i) : deque(d), index(i) {}
    operator array_deque_iterator<const ValueType, const ArrayDeque>() { return {deque, index}; }

    reference operator[](size_t i) const { return (*deque)[index + i]; }
    reference operator*() const { return (*deque)[index]; }
    pointer operator->() const { return &(*deque)[index]; }

    iterator operator--() { --index; return *this; }
    iterator operator++() { ++index; return *this; }

    iterator operator--(int) { return {deque, index--}; }
    iterator operator++(int) { return {deque, index++}; }

    iterator operator+(size_t a) const { return {deque, index + a}; }
    iterator operator-(size_t a) const { return {deque, index - a}; }

    iterator& operator+=(size_t a) { index += a; return *this; }
    iterator& operator-=(size_t a) { index -= a; return *this; }

    std::ptrdiff_t operator-(iterator o) const { return index - o.index; }

    bool operator<(iterator o) const { return index < o.index; }
    bool operator>(iterator o) const { return index > o.index; }
    bool operator<=(iterator o) const { return index <= o.index; }
    bool operator>=(iterator o) const { return index >= o.index; }

    bool operator==(iterator o) const { return index == o.index; }
    bool operator!=(iterator o) const { return index != o.index; }
};

template <typename V, typename D>
array_deque_iterator<V, D> operator+(size_t a, array_deque_iterator<V, D> it) { return {it.deque, a + it.index}; }
template <typename V, typename D>
array_deque_iterator<V, D> operator-(size_t a, array_deque_iterator<V, D> it) { return {it.deque, a - it.index}; }

} // namespace detail

// TODO move header and test to a separate git repository (and import it into mono)
// TODO exception safety review!
// TODO extension: non-destroying (on pop_XXX)
// TODO extension: small capacity optimization (with custom inline storage)
// TODO extension: no start field
template <typename T,
          typename Allocator = std::allocator<T>,
          typename Size = unsigned int,
          typename growth = detail::default_growth<Size>>
class array_deque {
   public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = Size;
    using reference = T&;
    using iterator = detail::array_deque_iterator<T, array_deque>;
    using const_iterator = detail::array_deque_iterator<const T, const array_deque>;
    using const_reference = const T&;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    array_deque() noexcept(noexcept(Allocator())) {}

    explicit array_deque(const Allocator& alloc) noexcept : m_alloc(alloc) {}

    explicit array_deque(size_type size)
            : m_alloc()
            , m_data(m_alloc.allocate(size))
            , m_capacity(size) {
        std::uninitialized_value_construct(m_data, m_data + size);
        m_size = size; // initialize after construction in case of exception
    }

    array_deque(size_type size, const T& init, const Allocator& alloc = Allocator())
            : m_alloc(alloc)
            , m_data(m_alloc.allocate(size))
            , m_capacity(size)
            , m_size(size) {
        std::uninitialized_fill(m_data, m_data + size, init);
        m_size = size; // initialize after construction in case of exception
    }

    template <class InputIt>
    array_deque(InputIt first, InputIt last, const Allocator& alloc = Allocator())
            : m_alloc(alloc)
            , m_data(m_alloc.allocate(std::distance(first, last)))
            , m_capacity(std::distance(first, last))
            , m_size(std::distance(first, last)) {
        detail::uninitialized_copy_const(first, last, m_data);
        m_size = std::distance(first, last); // initialize after construction in case of exception
    }

    array_deque(const array_deque& o)
            : array_deque(o.cbegin(), o.cend(), std::allocator_traits<allocator_type>::select_on_container_copy_construction(o.get_allocator())) {}

    array_deque(const array_deque& o, const Allocator& alloc)
            : array_deque(o.cbegin(), o.cend(), alloc) {}

    array_deque(array_deque&& o) noexcept
            : m_alloc(std::move(o.m_alloc))
            , m_data(std::move(o.m_data))
            , m_capacity(o.m_capacity)
            , m_start(o.m_start)
            , m_size(o.m_size) {
        o.m_data = nullptr;
        o.m_capacity = 0;
        o.m_start = 0;
        o.m_size = 0;
    }

    array_deque(array_deque&& o, const Allocator& alloc)
            : m_alloc(alloc) {
        if (m_alloc == o.get_allocator()) {
            m_data = std::move(o.m_data);
            m_capacity = o.m_capacity;
            m_start = o.m_start;
            m_size = o.m_size;
            o.m_data = nullptr;
            o.m_capacity = 0;
            o.m_start = 0;
            o.m_size = 0;
        } else {
            m_data = m_alloc.allocate(o.size());
            m_capacity = o.size();
            m_start = 0;
            std::uninitialized_move(o.begin(), o.end(), m_data);
            m_size = o.size(); // initialize after construction in case of exception
        }
    }

    array_deque(std::initializer_list<T> init, const Allocator& alloc = Allocator())
            : m_alloc(alloc)
            , m_data(m_alloc.allocate(init.size()))
            , m_capacity(init.size()) {
        detail::uninitialized_copy_const(init.begin(), init.end(), m_data);
        m_size = init.size(); // initialize after construction in case of exception
    }

    ~array_deque() {
        clear();
        m_alloc.deallocate(m_data, m_capacity);
    }

    // TODO allocator propagation or not
    array_deque& operator=(const array_deque& o) {
        if (this != &o) assign(o.cbegin(), o.cend());
        return *this;
    }

    // TODO allocator propagation or not
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

    array_deque& operator=(std::initializer_list<T> ilist) {
        assign(std::move(ilist));
        return *this;
    }

    void assign(size_type count, const T& value) {
        if (count > m_capacity) {
            // grow
            clear();
            resize(count, value);
            return;
        }

        // inplace
        if (count < m_size) {
            std::fill(begin(), begin() + count, value);
            std::destroy(begin() + count, end());
        } else {
            std::fill(begin(), begin() + m_size, value);
            std::uninitialized_value_construct(begin() + m_size, begin() + count);
        }
        m_size = count;
    }

    template <class InputIt>
    void assign(InputIt first, InputIt last) {
        auto count = std::distance(first, last);
        if (count > m_capacity) {
            // grow
            clear();
            realloc(count);
            detail::uninitialized_copy_const(first, last, m_data);
            m_size = count;
            return;
        }

        // inplace
        if (count < m_size) {
            std::copy(first, last, begin());
            std::destroy(begin() + count, end());
        } else {
            for (int i = 0; i < m_size; i++) begin()[i] = *first++;
            std::uninitialized_copy(first, last, begin() + m_size);
        }
        m_size = count;
    }

    void assign(std::initializer_list<T> ilist) { assign(ilist.begin(), ilist.end()); }

    constexpr allocator_type get_allocator() const noexcept { return m_alloc; }

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
    constexpr T& back() { return m_data[offset(m_size - 1)]; }

    constexpr const T& front() const { return m_data[m_start]; }
    constexpr const T& back() const { return m_data[offset(m_size - 1)]; }

    constexpr iterator begin() noexcept { return {this, 0}; }
    constexpr iterator end() noexcept { return {this, m_size}; }

    constexpr const_iterator begin() const noexcept { return {this, 0}; }
    constexpr const_iterator end() const noexcept { return {this, m_size}; }

    constexpr const_iterator cbegin() const noexcept { return {this, 0}; }
    constexpr const_iterator cend() const noexcept { return {this, m_size}; }

    constexpr reverse_iterator rbegin() noexcept { return end(); }
    constexpr reverse_iterator rend() noexcept { return begin(); }

    constexpr const_reverse_iterator crbegin() const noexcept { return end(); }
    constexpr const_reverse_iterator crend() const noexcept { return begin(); }

    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

    constexpr size_type size() const noexcept { return m_size; }

    constexpr size_type max_size() const noexcept { return std::numeric_limits<size_type>::max(); }

    constexpr void reserve(size_type new_capacity) {
        if (new_capacity > capacity()) realloc(new_capacity);
    }

    constexpr size_type capacity() const noexcept { return m_capacity; }

    void shrink_to_fit() {
        if (m_size < capacity()) realloc(m_size);
    }

    void clear() noexcept {
        std::destroy(begin(), end());
        m_size = 0;
    }

    iterator insert(const_iterator pos, const T& value) {
        iterator it(this, pos.index);
        if (expand(it, 1)) {
            *it = value;
        } else {
            new (m_data + offset(it.index)) T(std::move(value));
        }
        return it;
    }

    iterator insert(const_iterator pos, T&& value) {
        iterator it(this, pos.index);
        if (expand(it, 1)) {
            *it = std::move(value);
        } else {
            new (m_data + offset(it.index)) T(std::move(value));
        }
        return it;
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        iterator it(this, pos.index);
        if (expand(it, count)) {
            std::fill(it, it + count, value);
        } else {
            std::uninitialized_value_construct(it, it + count, value);
        }
        return it;
    }

    template <class InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        iterator it(this, pos.index);
        if (expand(it, std::distance(first, last))) {
            std::copy(first, last, it);
        } else {
            uninitialized_copy_const(first, last, it);
        }
        return it;
    }

    iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
        return insert(pos, ilist.begin(), ilist.end());
    }

    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        if (pos.index == 0) {
            emplace_front(args...);
            return begin();
        }
        if (pos.index == m_size) {
            emplace_back(args...);
            return {this, pos.index};
        }

        iterator it(this, pos.index);
        expand(it, 1);
        T* p = m_data + offset(it.index);
        std::destroy_at(p);
        new (p) T(args...);
        return it;
    }

    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }

    iterator erase(const_iterator first, const_iterator last) {
        const auto count = last.index - first.index;
        if (first.index < m_size - last.index) {
            // move front to right
            std::move_backward(begin(), begin() + first.index, begin() + last.index);
            std::destroy(begin(), begin() + count);
            m_start += count;
        } else {
            // move back to left
            std::move(begin() + last.index, end(), begin() + first.index);
            std::destroy(end() - count, end());
        }
        m_size -= count;
        return {this, first.index};
    }

    void push_front(const T& value) { emplace_front(value); }

    void push_front(T&& value) {
        ensure_space(1);
        size_type i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (m_data + i) T(std::move(value));
        m_size += 1;
        m_start = i;
    }

    template<class... Args>
    void emplace_front(Args&&... args) {
        ensure_space(1);
        size_type i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (m_data + i) T(args...);
        m_size += 1;
        m_start = i;
    }

    void pop_front() {
        size_type i = m_start++;
        if (m_start >= capacity()) m_start -= capacity();
        std::destroy_at(m_data + i);
        m_size -= 1;
    }

    void push_back(const T& value) { emplace_back(value); }

    void push_back(T&& value) {
        ensure_space(1);
        size_type i = offset(m_size);
        new (m_data + i) T(std::move(value));
        m_size += 1;
    }

    template<class... Args>
    void emplace_back(Args&&... args) {
        ensure_space(1);
        size_type i = offset(m_size);
        new (m_data + i) T(args...);
        m_size += 1;
    }

    void pop_back() {
        size_type i = offset(m_size - 1);
        std::destroy_at(m_data + i);
        m_size -= 1;
    }

    void resize(size_type count) {
        if (count > m_size) {
            ensure_space(count - m_size);
            std::uninitialized_value_construct(end(), end() + count - m_size);
            m_size = count;
            return;
        }
        std::destroy(begin() + count, end());
        m_size = count;
    }

    void resize(size_type count, const value_type& value) {
        if (count > m_size) {
            ensure_space(count - m_size);
            std::uninitialized_fill(end(), end() + count - m_size, value);
            m_size = count;
            return;
        }
        std::destroy(begin() + count, end());
        m_size = count;
    }

    constexpr void swap(array_deque& o)
            noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value
            || std::allocator_traits<Allocator>::is_always_equal::value) {
        if (std::allocator_traits<allocator_type>::propagate_on_container_swap::value) std::swap(m_alloc, o.m_alloc);
        std::swap(m_data, o.m_data);
        std::swap(m_capacity, o.m_capacity);
        std::swap(m_start, o.m_start);
        std::swap(m_size, o.m_size);
    }

    constexpr bool operator!=(const array_deque& o) const { return !operator==(o); }
    constexpr bool operator==(const array_deque& o) const {
        return m_size == o.size() && std::equal(cbegin(), cend(), o.cbegin());
    }

    constexpr bool operator<(const array_deque& o) const { return lex<std::less<T>>(o); }
    constexpr bool operator<=(const array_deque& o) const { return lex<std::less_equal<T>>(o); }
    constexpr bool operator>(const array_deque& o) const { return lex<std::greater<T>>(o); }
    constexpr bool operator>=(const array_deque& o) const { return lex<std::greater_equal<T>>(o); }

   private:
    template<typename Pred>
    constexpr bool lex(const array_deque& o) const {
        return std::lexicographical_compare(cbegin(), cend(), o.cbegin(), o.cend(), Pred());
    }

    size_type offset(Size index) const {
        size_type o = m_start + index;
        if (o >= capacity()) o -= capacity();
        return o;
    }

    void ensure_space(size_t space) {
        if (m_size + space > capacity()) reserve(growth()(capacity() + space));
    }

    void realloc(size_t new_capacity) {
        if (m_size == 0) {
            m_alloc.deallocate(m_data, m_capacity);
            m_data = m_alloc.allocate(new_capacity);
            m_capacity = new_capacity;
            m_start = 0;
            return;
        }

        array_deque<T, Allocator, Size, growth> temp;
        swap(temp);

        m_data = m_alloc.allocate(new_capacity);
        m_capacity = new_capacity;
        m_start = 0;
        m_size = temp.size();
        std::uninitialized_move(temp.begin(), temp.end(), begin());
    }

    void print(std::string name) {
        std::cout << name << std::endl;
        for (int i = 0; i < m_capacity; i++) {
            bool init1 = (m_start <= i && i < m_start + m_size && m_start + m_size <= m_capacity);
            bool init2 = (m_start + m_size > m_capacity) && (i < m_start + m_size - m_capacity || i >= m_start);
            if (init1 || init2) std::cout << m_data[i] << ' '; else std::cout << "# ";
        }
        std::cout << std::endl;
    }

    bool expand(iterator pos, size_t count) {
        if (m_size + count > m_capacity) {
            print("expand grow");
            array_deque<T, Allocator, Size, growth> temp;
            temp.realloc(growth()(m_size + count));
            std::uninitialized_move(begin(), pos, temp.begin());
            std::uninitialized_move(pos, end(), temp.begin() + pos.index + count);
            temp.m_size = m_size + count;
            swap(temp);
            return false;
        }

        /*if (pos.index == m_size) {
            print("expand inline back_simple");
            m_size += count;
            return false;
        }
        if (pos.index == 0) {
            print("expand inline front_simple");
            if (m_start < count) m_start += m_capacity;
            m_start -= count;
            m_size += count;
            return false;
        }*/

        auto before = pos.index;
        auto after = m_size - pos.index;
        if (before < after) {
            print("expand inline front");
            if (m_start < count) m_start += m_capacity;
            m_start -= count;
            m_size += count;

            std::uninitialized_move(begin() + count, begin() + count + count, begin());
            print("inline B");
            std::move(begin() + count + count, begin() + count + pos.index, begin() + count);
        } else {
            print("expand inline back");
            m_size += count;

            detail::uninitialized_move_backward(end() - count - count, end() - count, end());
            print("inline B");
            std::move_backward(pos, end() - count, end());
        }
        print("end expand");
        return true;
    }

   private:
    Allocator m_alloc;
    T* m_data = nullptr;
    Size m_capacity = 0;
    Size m_start = 0;
    Size m_size = 0;
};

// Deduction guide
template<class InputIt, class Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
array_deque(InputIt, InputIt, Alloc = Alloc()) -> array_deque<typename std::iterator_traits<InputIt>::value_type, Alloc>;

} // namespace mt

namespace std {

template<typename ... Args>
struct front_insert_iterator<mt::array_deque<Args...>> {
    using Container = mt::array_deque<Args...>;

    constexpr front_insert_iterator() noexcept : m_c(nullptr) { }

    explicit constexpr front_insert_iterator(Container& c) : m_c(&c) {}

    constexpr front_insert_iterator<Container>& operator=(const typename Container::value_type& value) {
        m_c->push_front(value);
        return *this;
    }

    constexpr front_insert_iterator<Container>& operator=(typename Container::value_type&& value) {
        m_c->push_front(value);
        return *this;
    }

    constexpr front_insert_iterator& operator*() { return *this; }
    constexpr front_insert_iterator& operator++() { return *this; }
    constexpr front_insert_iterator& operator++(int) { return *this; }

private:
    Container* m_c;
};

template<typename ... Args>
struct back_insert_iterator<mt::array_deque<Args...>> {
    using Container = mt::array_deque<Args...>;

    constexpr back_insert_iterator() noexcept : m_c(nullptr) { }

    explicit constexpr back_insert_iterator(Container& c) : m_c(&c) {}

    constexpr back_insert_iterator<Container>& operator=(const typename Container::value_type& value) {
        m_c->push_back(value);
        return *this;
    }

    constexpr back_insert_iterator<Container>& operator=(typename Container::value_type&& value) {
        m_c->push_back(value);
        return *this;
    }

    constexpr back_insert_iterator& operator*() { return *this; }
    constexpr back_insert_iterator& operator++() { return *this; }
    constexpr back_insert_iterator& operator++(int) { return *this; }

private:
    Container* m_c;
};

template <typename ... Args>
constexpr void swap(mt::array_deque<Args...>& a, mt::array_deque<Args...>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

template <class U, typename ... Args>
constexpr typename mt::array_deque<Args...>::size_type erase(mt::array_deque<Args...>& c, const U& value) {
    auto it = std::remove(c.begin(), c.end(), value);
    auto r = c.size() - it.index;
    c.erase(it, c.end());
    return r;
}

template <class Pred, typename ... Args>
constexpr typename mt::array_deque<Args...>::size_type erase_if(mt::array_deque<Args...>& c, const Pred& pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    auto r = c.size() - it.index;
    c.erase(it, c.end());
    return r;
}

template<typename ... Args>
struct hash<mt::array_deque<Args...>> {
    size_t operator()(const mt::array_deque<Args...>& a) const {
        size_t s = 0;
        std::hash<typename mt::array_deque<Args...>::value_type> ehash;
        for (const auto& e : a) s ^= ehash(e) + 0x9e3779b9 + (s << 6) + (s >> 2);
        return s;
    }
};

}
