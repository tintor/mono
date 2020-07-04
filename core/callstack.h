#pragma once
#include <array>
#include <memory>
#include <string>

class Callstack {
   public:
    Callstack();
    void write(std::string& out, const std::initializer_list<std::string_view>& exclude) const;

   private:
    std::array<void*, 20> _stack;
    int _size;
};

void InitSegvHandler();
