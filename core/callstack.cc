#include <cxxabi.h>
#include <execinfo.h>
#include <signal.h>
#include <exception>
#include <iostream>

#include "core/auto.h"
#include "core/callstack.h"
#include "core/string_util.h"
#include "core/util.h"
#include "core/numeric.h"

using namespace std;
using namespace std::literals;

Callstack::Callstack() { _size = backtrace(_stack.data(), _stack.size()); }

static bool matches(const initializer_list<string_view>& prefixes, string_view s) {
    for (string_view m : prefixes)
        if (s.size() >= m.size() && s.substr(0, m.size()) == m) return true;
    return false;
}

void Callstack::write(string& out, const initializer_list<string_view>& exclude) const {
    char** strs = backtrace_symbols(_stack.data(), _size);
    ON_SCOPE_EXIT(free(strs));

    FOR(i, _size) {
        auto sp = split(strs[i]);
        if (sp.size() <= 3) {
            out += strs[i];
            out += '\n';
            continue;
        }

        char* mangled_symbol = const_cast<char*>(sp[3].data());
        mangled_symbol[sp[3].size()] = '\0';
        string symbol = Demangle(mangled_symbol);

        if (symbol != "Callstack::Callstack()"sv && !matches(exclude, symbol)) {
            FOR(j, sp.size()) {
                out += (j == 3 && !symbol.empty()) ? string_view(symbol) : sp[j];
                out += ' ';
            }
            out += '\n';
        }
    }
}

static void sigsegv_handler(int sig) {
    fprintf(stderr, "Error: signal %d:\n", sig);
    Callstack stack;
    string s;
    stack.write(s, {"sigsegv_handler(int)", "_sigtramp"});
    fputs(s.c_str(), stderr);
    exit(1);
}

namespace std {

void terminate() noexcept {
    auto eptr = std::current_exception();
    try {
        if (eptr) std::rethrow_exception(eptr);
        std::cout << "terminate() called without exception" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception:" << std::endl << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unhandled exception of type: " << Demangle(abi::__cxa_current_exception_type()->name()) << std::endl;
    }
    std::abort();
}

};  // namespace std

void InitSegvHandler() {
    signal(SIGSEGV, sigsegv_handler);
    signal(SIGILL, sigsegv_handler);
}
