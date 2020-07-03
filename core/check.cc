#include "core/check.h"

#include <fmt/core.h>
#include <mutex>
#include <iostream>

std::mutex g_cout_mutex;

void Check(bool value, std::string_view message, const char* file, unsigned line, const char* function) {
    if (value) return;

    {
        std::unique_lock lock(g_cout_mutex);
        fmt::print("Check failed in {} at {}:{} with message: {}\n", function, file, line, message);
        // Callstack().write(s, {});
        std::cout.flush();
    }
    exit(0);
}

void Fail(std::string_view message, const char* file, unsigned line, const char* function) {
    {
        std::unique_lock lock(g_cout_mutex);
        fmt::print("Failed in {} at {}:{} with message: {}\n", function, file, line, message);
        // Callstack().write(s, {});
        std::cout.flush();
    }
    exit(0);
}
