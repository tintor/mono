#include "core/timestamp.h"
#include "core/callstack.h"
#include "core/vector.h"
#include "core/fmt.h"
#include "core/exception.h"
#include "core/array_deque.h"

#include <random>
#include <queue>
#include <optional>

using mt::array_deque;
using std::priority_queue;
using std::vector;
using std::array;
using std::optional;
using std::nullopt;

using Terms = vector<array<short, 3>>;
using Vars = vector<short>;

void Print(const Terms& problem) {
    for (auto [a, b, c] : problem) {
      if (a) print("{} ", a);
      if (b) print("{} ", b);
      if (c) print("{} ", c);
      print(", ");
    }
    print("\n");
}

void Print(const Vars& solution) {
    for (auto s : solution) {
        print("{} ", s);
    }
    print("\n");
}

/*bool Consistent(int vars, int a, Vars& solution) {
    if (abs(a) > vars) return true;
    if (1 <= a && a <= vars && solution[a - 1] > 0) return true;
    if (-1 >= a && a >= -vars && solution[-a - 1] < 0) return true;
    return false;
}

bool Consistent(const int vars, const Terms& problem, Vars& solution) {
    for (const auto& term : problem) {
        if (Consistent(vars, term[0], solution)) continue;
        if (Consistent(vars, term[1], solution)) continue;
        if (Consistent(vars, term[2], solution)) continue;
        return false;
    }
    return true;
}

bool Recurse(const int index, const int vars, const Terms& problem, Vars& solution) {
    if (index == vars) return true;
    solution[index] = index + 1;
    if (Consistent(index + 1, problem, solution) && Recurse(index + 1, vars, problem, solution)) return true;
    solution[index] = -(index + 1);
    if (Consistent(index + 1, problem, solution) && Recurse(index + 1, vars, problem, solution)) return true;
    return false;
}

size_t CountRecurse(const int index, const int vars, const Terms& problem, Vars& solution) {
    if (index == vars) return 1;

    size_t count = 0;

    solution[index] = index + 1;
    if (Consistent(index + 1, problem, solution)) count += CountRecurse(index + 1, vars, problem, solution);

    solution[index] = -(index + 1);
    if (Consistent(index + 1, problem, solution)) count += CountRecurse(index + 1, vars, problem, solution);

    return count;
}

Vars Solve(const Terms& problem, int vars) {
    Vars solution(vars);
    if (Recurse(0, vars, problem, solution)) return solution;
    return {};
}

size_t NumSolutions(const Terms& problem, int vars) {
    Vars solution(vars);
    return CountRecurse(0, vars, problem, solution);
}*/

// ---------------------------------------------

struct Entry {
    Vars solution;
    Terms terms;
};

array_deque<short> Q;

optional<Entry> MakeEntry(int m, const Terms& terms, const Vars& solution) {
    Entry e;
    e.solution = solution;
    e.solution[abs(m) - 1] = m;
    e.terms = terms;

    Q.clear();
    Q.push_back(m);
    while (!Q.empty()) {
        m = Q.front();
        Q.pop_front();

        int w = 0;
        for (int r = 0; r < e.terms.size(); r++) {
            auto [a, b, c] = e.terms[r];
            if (a == m || b == m || c == m) continue;
            if (a == -m) {
                a = b;
                b = c;
                c = 0;
            } else if (b == -m) {
                b = c;
                c = 0;
            } else if (c == -m) {
                c = 0;
            }

            if (c != 0) {
                e.terms[w++] = {a, b, c};
                continue;
            }
            if (b != 0) {
                e.terms[w++] = {a, b, 0};
                continue;
            }
            if (a == 0) THROW(runtime_error, "bug");
            // since only one var is remaining in term, move it to solution
            if (e.solution[abs(a) - 1] == -a) return nullopt;
            if (e.solution[abs(a) - 1] == 0) Q.push_back(a);
            e.solution[abs(a) - 1] = a;
        }
        e.terms.resize(w);
    }
    // TODO sort and remove dups
    return e;
}

// TODO alternative queue orders: smallest number of terms of size 3
// TODO alternative heuristics for selecting M smartly (cycle through different heuristics)
// TODO before push: if any literal is only positive OR only negative: remove terms in-place
// TODO before push: sort and remove dups
// TODO before push: if there are all 4 combinations of two literals at size 2 term => UNSAT
// TODO before push: if there are only 3 combinations of two literals at size 2 term => simplify in-place
Vars SolveWithQueue(const Terms& problem, int vars) {
    const auto cmp = [](const Entry& a, const Entry& b) {
        return b.terms.size() < a.terms.size();
    };
    priority_queue<Entry, vector<Entry>, decltype(cmp)> queue(cmp);
    Vars empty(vars, 0);
    queue.push({empty, problem});

    vector<int> count;
    count.resize(vars * 2 + 1);

    long q = 0;
    while (!queue.empty()) {
        auto solution = std::move(queue.top().solution);
        Terms terms = std::move(queue.top().terms);
        queue.pop();
        if (++q % (1 << 14) == 0) print("terms {}, queue {}\n", terms.size(), queue.size());

        //print("popped\n");
        //Print(solution);
        //Print(terms);

        if (terms.empty()) {
            for (int i = 0; i < vars; i++) if (solution[i] == 0) solution[i] = i + 1;
            return solution;
        }

        // select unsolved var smartly
        std::fill(count.begin(), count.end(), 0);
        for (auto [a, b, c] : terms) {
            count[a + vars] += 1;
            count[b + vars] += 1;
            if (c != 0) count[c + vars] += 1;
        }
        int m = 0;
        for (int i = 0; i < count.size(); i++) {
            if (count[i] > count[m]) m = i;
        }
        m -= vars;
        if (m == 0) THROW(runtime_error, "bug");
        // branch
        if (count[m + vars] > 0) {
            auto e = MakeEntry(m, terms, solution);
            if (e) {
                //print("pushing\n");
                //Print(e->solution);
                //Print(e->terms);
                queue.push(*e);
            }
        }
        if (count[-m + vars] > 0) {
            auto e = MakeEntry(-m, terms, solution);
            if (e) {
                //print("pushing\n");
                //Print(e->solution);
                //Print(e->terms);
                queue.push(*e);
            }
        }
    }
    return {};
}

int RandVar(int vars, std::mt19937& generator) {
    std::uniform_int_distribution<> dist(1, vars);
    std::uniform_int_distribution<> dist2(0, 1);
    return dist(generator) * (dist2(generator) * 2 - 1);
}

bool HasDuplicate(const Terms& problem) {
    for (int i = 0; i < problem.size() - 1; i++) {
        if (problem[i] == problem.back()) return true;
    }
    return false;
}

Terms GenerateTerms(int vars, int terms) {
    Terms problem(terms);
    std::random_device device;
    std::mt19937 generator(device());
    for (auto& [a, b, c] : problem) {
        a = RandVar(vars, generator);
        b = RandVar(vars, generator);
        while (a == b || a == -b) b = RandVar(vars, generator);

        c = RandVar(vars, generator);
        while (a == c || a == -c || b == c || b == -c || HasDuplicate(problem)) c = RandVar(vars, generator);
    }
    return problem;
}

bool Consistent(array<int, 3> term, const Vars& solution) {
    if (int a = term[0]; (a > 0 && solution[a - 1] > 0) || (a < 0 && solution[-a - 1] < 0)) return true;
    if (int a = term[1]; (a > 0 && solution[a - 1] > 0) || (a < 0 && solution[-a - 1] < 0)) return true;
    if (int a = term[2]; (a > 0 && solution[a - 1] > 0) || (a < 0 && solution[-a - 1] < 0)) return true;
    return false;
}

bool IsValidSolution(const Terms& problem, const Vars& solution) {
    for (auto [a, b, c] : problem) {
        if (a == 0 || abs(a) > solution.size()) THROW(runtime_error, "var out of range");
        if (b == 0 || abs(b) > solution.size()) THROW(runtime_error, "var out of range");
        if (c == 0 || abs(c) > solution.size()) THROW(runtime_error, "var out of range");

        if ((a > 0 && solution[a - 1] > 0) || (a < 0 && solution[-a - 1] < 0)) continue;
        if ((b > 0 && solution[b - 1] > 0) || (b < 0 && solution[-b - 1] < 0)) continue;
        if ((c > 0 && solution[c - 1] > 0) || (c < 0 && solution[-c - 1] < 0)) continue;
        return false;
    }
    return true;
}

Terms GenerateTermsWithSolution(int vars, int terms) {
    std::random_device device;
    std::mt19937 generator(device());

    std::uniform_int_distribution<> dist2(0, 1);
    Vars solution(vars);

    Terms problem;
    problem.reserve(terms);

    for (int i = 0; i < vars; i++) solution[i] = (i + 1) * (dist2(generator) * 2 - 1);
    problem.clear();

    for (int i = 0; i < terms; i++) {
        problem.push_back({0, 0, 0});
        auto& term = problem[i];
        auto& [a, b, c] = term;

        a = RandVar(vars, generator);
        b = RandVar(vars, generator);
        while (a == b || a == -b) b = RandVar(vars, generator);

        c = RandVar(vars, generator);
        while (a == c || a == -c || b == c || b == -c || !Consistent({a, b, c}, solution) || HasDuplicate(problem)) {
            c = RandVar(vars, generator);
        }
    }
    if (!IsValidSolution(problem, solution)) THROW(runtime_error, "bad problem");

    print("Secret solution:\n");
    Print(solution);
    return problem;
}

// 250 -> 21s
int main(int argc, char* argv[]) {
    InitSegvHandler();
    int vars = 300;
    ulong ticks = 0;
    for (int i = 1; i <= 100; i++) {
        auto problem = GenerateTermsWithSolution(vars, vars * 4.25);

        Timestamp start;
        auto solution = SolveWithQueue(problem, vars);
        ticks += start.elapsed();
        print("\n");
        if (solution.empty()) {
            print("No solution!\n");
            break;
        } else {
            if (!IsValidSolution(problem, solution)) THROW(runtime_error, "bad solution");
            print("Solution:\n");
            Print(solution);
        }

        print("{} problems in average {:.1f} seconds\n\n", i, Timestamp::to_s(ticks) / i);
    }
    return 0;
}
