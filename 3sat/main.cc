#include "core/timestamp.h"
#include "core/callstack.h"
#include "core/vector.h"
#include "core/fmt.h"
#include "core/exception.h"
#include <random>
using std::vector;
using std::array;

using Problem = vector<array<int, 3>>;

bool Consistent(int vars, int a, vector<char>& solution) {
    if (abs(a) > vars) return true;
    if (1 <= a && a <= vars && solution[a - 1]) return true;
    if (-1 >= a && a >= -vars && !solution[-a - 1]) return true;
    return false;
}

bool Consistent(const int vars, const Problem& problem, vector<char>& solution) {
    for (const auto& term : problem) {
        if (Consistent(vars, term[0], solution)) continue;
        if (Consistent(vars, term[1], solution)) continue;
        if (Consistent(vars, term[2], solution)) continue;
        return false;
    }
    return true;
}

bool Recurse(const int index, const int vars, const Problem& problem, vector<char>& solution) {
    if (index == vars) return true;
    solution[index] = true;
    if (Consistent(index + 1, problem, solution) && Recurse(index + 1, vars, problem, solution)) return true;
    solution[index] = false;
    if (Consistent(index + 1, problem, solution) && Recurse(index + 1, vars, problem, solution)) return true;
    return false;
}

size_t CountRecurse(const int index, const int vars, const Problem& problem, vector<char>& solution) {
    if (index == vars) return 1;

    size_t count = 0;

    solution[index] = true;
    if (Consistent(index + 1, problem, solution)) count += CountRecurse(index + 1, vars, problem, solution);

    solution[index] = false;
    if (Consistent(index + 1, problem, solution)) count += CountRecurse(index + 1, vars, problem, solution);

    return count;
}

vector<char> Solve(const Problem& problem, int vars) {
    vector<char> solution(vars);
    if (Recurse(0, vars, problem, solution)) return solution;
    return {};
}

size_t NumSolutions(const Problem& problem, int vars) {
    vector<char> solution(vars);
    return CountRecurse(0, vars, problem, solution);
}

int RandVar(int vars, std::mt19937& generator) {
    std::uniform_int_distribution<> dist(1, vars);
    std::uniform_int_distribution<> dist2(0, 1);
    return dist(generator) * (dist2(generator) * 2 - 1);
}

Problem GenerateProblem(int vars, int terms) {
    Problem problem(terms);
    std::random_device device;
    std::mt19937 generator(device());
    for (auto& [a, b, c] : problem) {
        a = RandVar(vars, generator);
        b = RandVar(vars, generator);
        while (a == b || a == -b) b = RandVar(vars, generator);

        c = RandVar(vars, generator);
        while (a == c || a == -c || b == c || b == -c) c = RandVar(vars, generator);
    }
    return problem;
}

void Print(const Problem& problem) {
    for (auto [a, b, c] : problem) {
      if (a) print("{} ", a);
      if (b) print("{} ", b);
      if (c) print("{} ", c);
      print(", ");
    }
    print("\n");
}

void Print(const vector<char>& solution) {
    for (int i = 1; i <= solution.size(); i++) print("{} ", solution[i - 1] ? i : -i);
    print("\n");
}

bool Consistent(array<int, 3> term, const vector<char>& solution) {
    if (int a = term[0]; (a > 0 && solution[a - 1]) || (a < 0 && !solution[-a - 1])) return true;
    if (int a = term[1]; (a > 0 && solution[a - 1]) || (a < 0 && !solution[-a - 1])) return true;
    if (int a = term[2]; (a > 0 && solution[a - 1]) || (a < 0 && !solution[-a - 1])) return true;
    return false;
}

bool IsValidSolution(const Problem& problem, const vector<char>& solution) {
    for (auto [a, b, c] : problem) {
        if (a == 0 || abs(a) > solution.size()) THROW(runtime_error, "var out of range");
        if (b == 0 || abs(b) > solution.size()) THROW(runtime_error, "var out of range");
        if (c == 0 || abs(c) > solution.size()) THROW(runtime_error, "var out of range");

        if ((a > 0 && solution[a - 1]) || (a < 0 && !solution[-a - 1])) continue;
        if ((b > 0 && solution[b - 1]) || (b < 0 && !solution[-b - 1])) continue;
        if ((c > 0 && solution[c - 1]) || (c < 0 && !solution[-c - 1])) continue;
        return false;
    }
    return true;
}

bool HasDuplicate(const Problem& problem) {
    for (int i = 0; i < problem.size() - 1; i++) {
        if (problem[i] == problem.back()) return true;
    }
    return false;
}

Problem GenerateProblemWithSolution(int vars, int terms) {
    std::random_device device;
    std::mt19937 generator(device());

    std::uniform_int_distribution<> dist2(0, 1);
    vector<char> solution(vars), copy;

    Problem problem;
    problem.reserve(terms);

again:
    for (char& v : solution) v = dist2(generator);
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

    copy = solution;
    for (int i = 0; i < vars; i++) {
        copy[i] = 1 - copy[i];
        if (IsValidSolution(problem, copy)) goto again;

        for (int j = i + 1; j < vars; j++) {
            copy[j] = 1 - copy[j];
            if (IsValidSolution(problem, copy)) goto again;

            for (int k = j + 1; k < vars; k++) {
                copy[k] = 1 - copy[k];
                if (IsValidSolution(problem, copy)) goto again;

                for (int m = k + 1; m < vars; m++) {
                    copy[m] = 1 - copy[m];
                    if (IsValidSolution(problem, copy)) goto again;
                    copy[m] = 1 - copy[m];
                }
                copy[k] = 1 - copy[k];
            }

            copy[j] = 1 - copy[j];
        }
        copy[i] = 1 - copy[i];
    }

    print("Secret solution:\n");
    Print(solution);
    return problem;
}

// Simple recursive solver: 100 random problems (with solution) with 47 vars (and x4.25 terms) in 3.3s on average
// Simple recursive solver: 100 random problems (dup removal + 3flip) (with solution) with 47 vars (and x4.25 terms) in 4.1s on average

int main(int argc, char* argv[]) {
    InitSegvHandler();
    int vars = 50;
    ulong ticks = 0;
    for (int i = 1; i <= 100; i++) {
        //auto p = GenerateProblem(vars, vars * 4.25);
        auto problem = GenerateProblemWithSolution(vars, vars * 4.25);
        print("Problem:\n");
        Print(problem);
        Timestamp start;
        auto solution = Solve(problem, vars);
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
