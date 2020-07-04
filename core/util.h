#pragma once
#include "core/span.h"

template<typename T = double>
inline T env(const char* name, T def) {
    const char* v = std::getenv(name);
    return v ? std::atof(v) : def;
}

std::string Demangle(const char* name);

template <typename T>
inline std::string TypeName(const T& value) {
    return Demangle(typeid(value).name());
}

void PrintTable(cspan<std::string> rows, char separator, std::string_view splitter, cspan<int> right = {});
