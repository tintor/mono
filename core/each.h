#pragma once
#include <cstddef>

/*
struct iter : public each<iter> {
        int pos = 0;
        optional<int> next() {
                return pos < 10 ? pos++ : optional<int>();
        }
};

void main() {
        for (auto e : iter())
                printf("%d\n", e);
}
*/

template <typename T>
class each {
    class internal {
        T* _it;
        decltype(_it->next()) _value;

       public:
        internal(T* it) : _it(it), _value(it->next()) {}
        auto operator*() { return *_value; }
        void operator++() { _value = _it->next(); }
        bool operator!=(std::nullptr_t) { return _value.has_value(); }
        auto& begin() { return *this; }
        std::nullptr_t end() { return nullptr; }
    };

   public:
    auto begin() { return internal(reinterpret_cast<T*>(this)); }
    std::nullptr_t end() { return nullptr; }
};

/*
struct iter {
        int pos = 0;
        optional<int> next() {
                return pos < 10 ? pos++ : optional<int>();
        }
};

void main() {
        for (auto e : iterable(iter()))
                printf("%d\n", e);
}
*/

template <typename T>
class iterable_t {
    T _it;
    decltype(_it.next()) _value;

   public:
    iterable_t(T it) : _it(std::move(it)), _value(_it.next()) {}
    auto operator*() { return *_value; }
    void operator++() { _value = _it.next(); }
    bool operator!=(std::nullptr_t) { return _value.has_value(); }
    auto& begin() { return *this; }
    std::nullptr_t end() { return nullptr; }
};

template <typename T>
auto iterable(T it) {
    return iterable_t(std::move(it));
}
