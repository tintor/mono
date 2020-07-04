#include <core/string_util.h>
#include <core/util.h>
#include <cxxabi.h>

string Demangle(const char* name) {
    int status;
    char* demangled = abi::__cxa_demangle(name, 0, 0, &status);
    string out = demangled;
    free(demangled);
    return out;
}

void PrintTable(cspan<string> rows, char separator, string_view splitter, cspan<int> right) {
    vector<size_t> columns;
    for (const string& row : rows) {
        auto s = split(row, separator, false);
        if (s.size() > columns.size()) columns.resize(s.size());
        for (size_t i = 0; i < s.size(); i++) columns[i] = max(columns[i], s[i].size());
    }
    vector<bool> is_right(columns.size(), false);
    for (auto e : right) is_right[e] = true;

    for (const string& row : rows) {
        auto s = split(row, separator, false);
        for (size_t i = 0; i < s.size(); i++) {
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
