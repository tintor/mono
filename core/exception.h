#pragma once
#include <exception>

#include "core/callstack.h"
#include "fmt/core.h"

struct ThrowLoc {
    const char* pretty;
    const char* function;
    const char* file;
    size_t line;
};

class exception : public std::exception {
   public:
    exception(ThrowLoc loc) : _loc(loc) {}

    // note: doesn't copy string!
    explicit exception(ThrowLoc loc, const char* what) : _loc(loc), _what_view(what) {}

    // note: doesn't copy string!
    explicit exception(ThrowLoc loc, std::string_view what) : _loc(loc), _what_view(what) {}

    explicit exception(ThrowLoc loc, std::string what) : _loc(loc), _what(what) {}

    template <typename T, typename... Args>
    exception(ThrowLoc loc, std::string_view fmt, T arg, Args... args) : _loc(loc) {
        format_s(_what, fmt, arg, args...);
    }

    virtual const char* what() const noexcept {
        if (_str.size() == 0) {
            auto class_name = Callstack::demangle(typeid(*this).name());
            _str += class_name.get();
            if (_what.size() > 0) format_s(_str, " %s", _what);
            if (_what_view.size() > 0) format_s(_str, " %s", _what_view);
            format_s(_str, "\n%s\n%s:%s\n", _loc.pretty, _loc.file, _loc.line);
            std::string cn = format("%s::%s<int>(", class_name.get(), class_name.get());
            _callstack.write(_str, {"exception::exception<int>(", cn});
        }
        return _str.c_str();
    }

   private:
    ThrowLoc _loc;
    Callstack _callstack;
    std::string _what;
    std::string_view _what_view;
    mutable std::string _str;
};

#define EXCEPTION(NAME)                                                                                      \
    struct NAME : public exception {                                                                         \
        NAME(ThrowLoc loc) : exception(loc) {}                                                               \
        explicit NAME(ThrowLoc loc, const char* what) : exception(loc, what) {}                              \
        explicit NAME(ThrowLoc loc, std::string_view what) : exception(loc, what) {}                         \
        explicit NAME(ThrowLoc loc, std::string what) : exception(loc, what) {}                              \
        template <typename T, typename... Args>                                                              \
        NAME(ThrowLoc loc, std::string_view fmt, T arg, Args... args) : exception(loc, fmt, arg, args...) {} \
    }

EXCEPTION(runtime_error);
EXCEPTION(overflow_error);
EXCEPTION(not_implemented);
EXCEPTION(invalid_argument);

#define THROW(TYPE, ...) throw TYPE(ThrowLoc{__PRETTY_FUNCTION__, __FUNCTION__, __FILE__, __LINE__}, ##__VA_ARGS__)

#define ASSERT_ALWAYS(E, ...)                          \
    {                                                  \
        if (!(E)) THROW(runtime_error, ##__VA_ARGS__); \
    }
