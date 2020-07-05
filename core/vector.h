#pragma once
#include <type_traits>
#include <vector>
#include <optional>
#include <iostream>

// Define vector with sizeof(vector) == 16 instead of 24
//#include <boost/container/vector.hpp>
//template <typename T, typename Allocator = std::allocator<T>>
//using vector =
//    boost::container::vector<T, Allocator,
//                             typename boost::container::vector_options<boost::container::stored_size<uint32_t>>::type>;
//static_assert(sizeof(vector<int>) == 16);

// vector implemented in array
#include <boost/container/static_vector.hpp>
using boost::container::static_vector;

// static storage for small number of elements
#include <boost/container/small_vector.hpp>
using boost::container::small_vector;

template <typename T>
void operator<<(std::vector<T>& v, const T& e) {
    v.push_back(e);
}

template <typename T, typename M, typename X, std::enable_if_t<!std::is_same_v<T, std::vector<X>>>>
void operator<<(std::vector<T>& v, const std::vector<M>& e) {
    v.insert(v.end(), e.begin(), e.end());
}

namespace std {

template<typename T>
ostream& operator<<(ostream& os, const optional<T>& a) {
    if (a.has_value()) return os << a.value();
    return os << "null";
}

template<typename T, typename A>
ostream& operator<<(ostream& os, const vector<T, A>& v) {
    os << '[';
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) os << ' ';
        os << v[i];
    }
    return os << ']';
}

}
