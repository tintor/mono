#pragma once
#include <algorithm>

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

template <typename ValueType, typename ArrayDeque>
struct array_deque_iterator {
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using pointer = ValueType*;
    using reference = ValueType&;
    using iterator_category = std::random_access_iterator_tag;

    using iterator = array_deque_iterator;
    ArrayDeque& deque;
    size_t index;

    array_deque_iterator(ArrayDeque& d, size_t i) : deque(d), index(i) {}
    iterator& operator=(const iterator& o) { index = o.index; return *this; }

    reference operator[](size_t i) const { return deque[index + i]; }
    reference operator*() const { return deque[index]; }
    pointer operator->() const { return &deque[index]; }

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

}

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
        clear();
        reserve(count);
        std::uninitialized_fill(m_data, m_data + count, value);
        m_size = count; // initialize after construction in case of exception
    }

    template <class InputIt>
    void assign(InputIt first, InputIt last) {
        clear();
        reserve(std::distance(first, last));
        detail::uninitialized_copy_const(first, last, m_data);
        m_size = std::distance(first, last); // initialize after construction in case of exception
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

    constexpr iterator begin() noexcept { return iterator(*this, 0); }
    constexpr iterator end() noexcept { return iterator(*this, m_size); }

    constexpr const_iterator begin() const noexcept { return {*this, 0}; }
    constexpr const_iterator end() const noexcept { return const_iterator(*this, m_size); }

    constexpr const_iterator cbegin() const noexcept { return {*this, 0}; }
    constexpr const_iterator cend() const noexcept { return const_iterator(*this, m_size); }

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
        while (m_size > 0) pop_back();
    }

    iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }

    iterator insert(const_iterator pos, T&& value) {
        iterator it(*this, pos.index);
        if (expand(it, 1)) {
            *it = value;
        } else {
            new (m_data + it.index) T(value);
        }
        return it;
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        iterator it(*this, pos.index);
        if (expand(it, count)) {
            std::fill(it, it + count, value);
        } else {
            std::uninitialized_value_construct(it, it + count, value);
        }
        return it;
    }

    template <class InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        iterator it(*this, pos.index);
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
        iterator it(*this, pos.index);
        if (expand(it, 1)) {
            T* p = m_data + offset(it.index);
            std::destroy_at(p);
            new (p) T(args...);
        } else {
            new (m_data + it.index) T(args...);
        }
        return it;
    }

    iterator erase(const_iterator pos) {
        iterator it(*this, pos.index);
        std::move(it + 1, end(), it);
        std::destroy_at(m_data + offset(m_size - 1));
        return it;
    }

    iterator erase(const_iterator first, const_iterator last) {
        const auto count = std::distance(first, last);
        iterator it(*this, first.index);
        std::move(it + count, end(), it);
        std::destroy(end() - count, end());
        return it;
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
    reference emplace_front(Args&&... args) {
        ensure_space(1);
        size_type i = ((m_start == 0) ? m_capacity : m_start) - 1;
        new (m_data + i) T(args...);
        m_size += 1;
        m_start = i;
        return m_data[i];
    }

    void pop_front() {
        size_type i = m_start++;
        if (m_start >= capacity()) m_start -= capacity();
        m_size -= 1; // decrement size before destroying it
        std::destroy_at(m_data + i);
    }

    void push_back(const T& value) { emplace_back(value); }

    void push_back(T&& value) {
        ensure_space(1);
        size_type i = offset(m_size);
        new (m_data + i) T(std::move(value));
        m_size += 1;
    }

    template<class... Args>
    reference emplace_back(Args&&... args) {
        ensure_space(1);
        size_type i = offset(m_size);
        new (m_data + i) T(args...);
        m_size += 1;
        return m_data[i];
    }

    void pop_back() {
        size_type i = offset(m_size - 1);
        m_size -= 1; // decrement size before destroying it
        std::destroy_at(m_data + i);
    }

    void resize(size_type count) {
        if (count > m_size) {
            ensure_space(count - m_size);
            std::uninitialized_value_construct(end(), end() + count);
            m_size = count;
            return;
        }
        while (count < size()) pop_back();
    }

    void resize(size_type count, const value_type& value) {
        if (count > m_size) {
            ensure_space(count - m_size);
            std::uninitialized_fill(end(), end() + count, value);
            m_size = count;
            return;
        }
        while (count < size()) pop_back();
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
            return;
        }

        array_deque<T, Allocator, Size, growth> temp;
        swap(temp);

        m_alloc.deallocate(m_data, m_capacity);
        m_data = m_alloc.allocate(new_capacity);
        m_capacity = new_capacity;

        while (temp.size() > 0) {
            new (m_data + m_size) T(std::move(temp.front()));
            m_size += 1;
            temp.pop_front();
        }
    }

    bool expand(iterator pos, size_t count) {
        if (m_size + count <= m_capacity) {
            std::move_backward(pos, end(), end() + count);
            m_size += count;
            return true;
        }

        array_deque<T, Allocator, Size, growth> temp;
        temp.reserve(growth()(capacity() + count));
        std::move(begin(), pos, temp.begin());
        std::move(pos, end(), temp.begin() + pos.index + count);
        temp.m_size = m_size + count;
        swap(temp);
        return false;
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

namespace std {

template<typename ... Args>
struct front_insert_iterator<array_deque<Args...>> {
    using Container = array_deque<Args...>;

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
struct back_insert_iterator<array_deque<Args...>> {
    using Container = array_deque<Args...>;

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
constexpr typename array_deque<T, Alloc>::size_type erase_if(array_deque<T, Alloc>& c, const Pred& pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    auto r = std::distance(it, c.end());
    c.erase(it, c.end());
    return r;
}

template<typename ... Args>
struct hash<array_deque<Args...>> {
    size_t operator()(const array_deque<Args...>& a) const {
        size_t s = 0;
        std::hash<typename array_deque<Args...>::value_type> ehash;
        for (const auto& e : a) s ^= ehash(e) + 0x9e3779b9 + (s << 6) + (s >> 2);
        return s;
    }
};

}
