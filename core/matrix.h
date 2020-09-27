#pragma once
#include <vector>
#include "core/numeric.h"
#include "core/exception.h"

// Row-major order
template <typename T>
class matrix {
   private:
    int _dim_a = 0;
    int _dim_b = 0;

    std::vector<T> _data;

   public:
    void resize(int rows, int cols) {
        if (rows < 0 || cols < 0) THROW(invalid_argument);
        _dim_a = rows;
        _dim_b = cols;
        _data.resize(rows * cols);
    }

    void resize_and_fill(int rows, int cols, T value) {
        if (rows < 0 || cols < 0) THROW(invalid_argument);
        _dim_a = rows;
        _dim_b = cols;
        _data.clear();
        _data.resize(rows * cols, value);
    }

    void fill(T value) {
        auto s = _data.size();
        _data.resize(0);
        _data.resize(s, value);
    }

    typename std::vector<T>::const_reference operator()(int2 v) const { return operator()(v.y, v.x); }

    typename std::vector<T>::reference operator()(int2 v) { return operator()(v.y, v.x); }

    typename std::vector<T>::const_reference operator()(int row, int col) const {
        if (row < 0 || row >= _dim_a || col < 0 || col >= _dim_b) THROW(invalid_argument);
        return _data[col * _dim_a + row];
    }

    typename std::vector<T>::reference operator()(int row, int col) {
        if (row < 0 || row >= _dim_a || col < 0 || col >= _dim_b) THROW(invalid_argument);
        return _data[col * _dim_a + row];
    }

    int rows() const { return _dim_a; }
    int cols() const { return _dim_b; }
    int2 shape() const { return int2{_dim_b, _dim_a}; }
};
