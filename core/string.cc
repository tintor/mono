#include <cxxabi.h>
#include <iostream>

#include "core/numeric.h"
#include "core/string.h"
using namespace std;

static void Replace(string& str, const string& from, const string& to) {
   while (true) {
     size_t i = str.find(from);
     if (i == string::npos) break;
     str.replace(i, from.length(), to);
   }
}

static void Replace(string& str, const regex& re, const string& fmt) {
   str = regex_replace(str, re, fmt);
}

string Demangle(const char* name) {
    int status = 1;
    char* demangled = abi::__cxa_demangle(name, 0, 0, &status);
    if (status != 0) return "";
    string out = demangled;
    free(demangled);

    Replace(out, regex("std::(__1|__cxx111)::"), "std::");
    Replace(out, "> >", ">>");

    Replace(out, "__int128", "cent");

    Replace(out, "unsigned long", "ulong");
    Replace(out, "unsigned int", "uint");
    Replace(out, "unsigned short", "ushort");
    Replace(out, "unsigned char", "uchar");
    Replace(out, "unsigned cent", "ucent");

    Replace(out, "std::chrono::duration<long long, std::ratio<1l, 1000000000l>>", "std::chrono::nanoseconds");

    Replace(out, "std::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string");
    Replace(out, "std::basic_string_view<char, std::char_traits<char>>", "std::string_view");

    Replace(out, regex("std::vector<(.+?), std::allocator<\\1>>"), "std::vector<$1>");
    Replace(out, regex("std::list<(.+?), std::allocator<\\1>>"), "std::list<$1>");

    Replace(out, regex("std::(unordered_)?(multi)?map<(.+?), (.+?), (std::hash<\\3>, std::equal_to<\\3>|std::less<\\3>), std::allocator<std::pair<\\3 const, \\4>>>"), "std::$1$2map<$3, $4>");
    Replace(out, regex("std::(unordered_)?(multi)?set<(.+?), (std::hash<\\3>, std::equal_to<\\3>|std::less<\\3>), std::allocator<\\3>>"),
        "std::$1$2set<$3>");
    return out;
}

void PrintTable(cspan<string> rows, char separator, string_view splitter, cspan<int> right) {
    vector<size_t> columns;
    for (const string& row : rows) {
        auto s = split(row, separator, false);
        if (s.size() > columns.size()) columns.resize(s.size());
        FOR(i, s.size()) columns[i] = std::max(columns[i], s[i].size());
    }
    vector<bool> is_right(columns.size(), false);
    for (auto e : right) is_right[e] = true;

    for (const string& row : rows) {
        auto s = split(row, separator, false);
        FOR(i, s.size()) {
            if (i > 0) cout << splitter;
            if (is_right[i])
                for (size_t j = s[i].size(); j < columns[i]; j++) cout << ' ';
            cout << s[i];
            if (!is_right[i])
                for (size_t j = s[i].size(); j < columns[i]; j++) cout << ' ';
        }
        cout << '\n';
    }
    cout.flush();
}
