#pragma once
#include <variant>

using X = unsigned long long;

X constexpr symbol_hash(char const* input) {
    if (!*input) return 0;
    X seed = symbol_hash(input + 1);
    return seed ^ (static_cast<X>(*input) + 0x9e3779b97f4a7c15 + (seed << 6) + (seed >> 2));
}

template<X T>
struct Symbol {
    template<X U>
    constexpr bool operator==(Symbol<U>) const { return T == U; }

    template<X U>
    constexpr bool operator!=(Symbol<U>) const { return T != U; }
};

template<typename ... args, X T>
constexpr bool operator==(std::variant<args...> a, Symbol<T> b) {
    return std::holds_alternative<Symbol<T>>(a);
}

template<typename ... args, X T>
constexpr bool operator!=(std::variant<args...> a, Symbol<T> b) {
    return !std::holds_alternative<Symbol<T>>(a);
}

// Type-safe enum which doesn't require type prefix in front of values like `enum class`.

// using True = Symbol<symbol_hash("true")>;
// using True = Symbol<symbol_hash("true")>;  <- no conflicts
// using False = Symbol<symbol_hash("false")>;
// using Maybe = Symbol<symbol_hash("maybe")>;
// std::variant<True, False, Maybe> e = True();
// if (e == True()) ....
