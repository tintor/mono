#pragma once
#include <type_traits>
#include <vector>

#include "absl/types/span.h"

template <typename T>
using span = absl::Span<T>;
template <typename T>
using cspan = span<const T>;

template <typename T, typename M> //, std::enable_if_t<(std::is_convertible_v<M, T>)>>
void operator<<(std::vector<T>& v, span<M> e) {
    v.insert(v.end(), e.begin(), e.end());
}
