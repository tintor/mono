#include "sokoban/level_env.h"
#include "core/exception.h"
#include "core/file.h"
#include "core/matrix.h"
#include <iostream>

using std::string_view;
using std::string;
using std::vector;

namespace Code {
constexpr char Box = '$';
constexpr char Wall = '#';
constexpr char BoxGoal = '*';
constexpr char AgentGoal = '+';
constexpr char Goal = '.';
constexpr char Agent = '@';
constexpr char Space = ' ';
};

int NumberOfLevels(string_view filename) {
    int currentLevelNo = 0;
    bool inside = false;
    for (auto line : FileReader(filename)) {
        if (line.empty())
            inside = false;
        else if (!inside) {
            currentLevelNo += 1;
            inside = true;
        }
    }
    return currentLevelNo;
}

static const std::regex level_suffix("(.+):(\\d+)");

static bool IsValidLine(string_view line) {
    for (char c : line)
        if (c != Code::Box && c != Code::Space && c != Code::Wall && c != Code::BoxGoal && c != Code::AgentGoal &&
            c != Code::Goal && c != Code::Agent)
            return false;
    return !line.empty();
}

static vector<string> LoadLevelLines(string_view filename) {
    int desiredLevelNo = 1;
    std::cmatch m;
    string temp;
    if (match(filename, level_suffix, m)) {
        temp = m[1].str();
        filename = temp;
        desiredLevelNo = std::stoi(m[2].str());
    }

    vector<string> lines;
    int currentLevelNo = 0;
    bool inside = false;
    for (auto line : FileReader(filename)) {
        if (!inside && IsValidLine(line)) {
            currentLevelNo += 1;
            inside = true;
        } else if (inside && !IsValidLine(line))
            inside = false;

        if (inside && currentLevelNo == desiredLevelNo) lines.push_back(string(line));
    }
    return lines;
}

void LevelEnv::Load(string_view filename) {
    auto lines = LoadLevelLines(filename);

    int cols = 0;
    for (const string& s : lines) cols = std::max(cols, int(s.size()));
    int rows = lines.size();

    wall.resize(rows, cols);
    wall.fill(false);

    box.resize(rows, cols);
    box.fill(false);

    goal.resize(rows, cols);
    goal.fill(false);

    agent = {-1, -1};
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            char c = (col < lines[row].size()) ? lines[row][col] : Code::Space;
            if (c == Code::AgentGoal || c == Code::Agent) agent = {col, row};
            if (c == Code::Box || c == Code::BoxGoal) box(row, col) = true;
            if (c == Code::Goal || c == Code::AgentGoal || c == Code::BoxGoal) goal(row, col) = true;
            if (c == Code::Wall) wall(row, col) = true;
        }
    }
}

bool equal(int2 a, int2 b) { return a.x == b.x && a.y == b.y; }

bool LevelEnv::IsValid() const {
    // All matrices must be the same size and at least 3.
    int2 shape = wall.shape();
    if (shape.x < 3 || shape.y < 3) return false;
    if (!equal(box.shape(), shape) || !equal(goal.shape(), shape)) return false;

    // Agent must be in a non-wall and non-box cell.
    if (agent.x < 0 || agent.x >= shape.x || agent.y >= shape.y || agent.y < 0) return false;
    if (wall(agent) || box(agent)) return false;

    // Boxes and goals must be in non-wall cells.
    for (int r = 0; r < shape.y; r++) for (int c = 0; c < shape.x; c++) {
        if (wall(r, c) && (box(r, c) || goal(r, c))) return false;
    }

    // There must be at least one goal for each box.
    int count = 0;
    for (int r = 0; r < shape.y; r++) for (int c = 0; c < shape.x; c++) {
        if (box(r, c)) count -= 1;
        if (goal(r, c)) count += 1;
    }
    return count >= 0;
}

string_view Emoji(bool wall, bool box, bool goal, bool agent) {
    if (wall) return "✴️ ";
    if (agent) return goal ? "😎" : "😀";
    if (box) return goal ? "🔵" : "🔴";
    if (goal) return "🏳 ";
    return "  ";
}

void LevelEnv::Print() const {
    for (int r = 0; r < wall.rows(); r++) {
        for (int c = 0; c < wall.cols(); c++) {
            int2 i{c, r};
            std::cout << Emoji(wall(i), box(i), goal(i), equal(agent, i));
        }
        std::cout << std::endl;
    }
}

void LevelEnv::Unprint() const {
    for (int r = 0; r < wall.rows(); r++) {
        std::cout << "\033[A"; // more cursor up
        std::cout << "\33[2K"; // erase current line
    }
    std::cout.flush();
}

bool LevelEnv::Action(int2 delta, bool allow_move, bool allow_push) {
    if (delta.x * delta.x + delta.y * delta.y != 1) THROW(runtime_error, "delta");

    int2 b = agent + delta;
    if (b.x < 0 || b.y < 0 || b.x >= wall.cols() || b.y >= wall.rows()) return false;
    if (wall(b)) return false;

    if (box(b)) {
        if (!allow_push || wall(b + delta) || box(b + delta)) return false;
        box(b + delta) = true;
        box(b) = false;
        agent = b;
        return true;
    }

    if (!allow_move) return false;
    agent = b;
    return true;
}

bool LevelEnv::IsSolved() const {
    for (int r = 0; r < wall.rows(); r++) for (int c = 0; c < wall.cols(); c++) {
        if (box(r, c) && !goal(r, c)) return false;
    }
    return true;
}
