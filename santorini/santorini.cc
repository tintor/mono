#include <random>
#include <variant>
#include <iostream>

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "core/random.h"
#include "core/callstack.h"
#include "core/model.h"
#include "core/thread.h"
#include "core/string.h"
#include "core/timestamp.h"
#include "core/vector.h"
#include "core/array_deque.h"

#include "santorini/action.h"
#include "santorini/board.h"
#include "santorini/minimax.h"

#include "view/font.h"
#include "view/glm.h"
#include "view/shader.h"
#include "view/vertex_buffer.h"
#include "view/window.h"

using namespace std;

// Board and judge
// ===============

optional<string_view> CanNext(const Board& board) {
    if (board.setup) {
        if (Count(board, L(e.figure == board.player)) != 2) return "need to place worker";
        return nullopt;
    }
    if (!board.moved) return "need to move";
    if (!board.built) return "need to build";
    return nullopt;
}

optional<string_view> Next(Board& board) {
    auto s = CanNext(board);
    if (s != nullopt) return s;
    board.player = Other(board.player);
    board.moved = std::nullopt;
    board.built = false;
    if (board.setup && Count(board, L(e.figure != Figure::None)) == 4) board.setup = false;
    return nullopt;
}

optional<string_view> CanPlace(const Board& board, Coord dest) {
    if (!IsValid(dest)) return "invalid coord";
    if (!board.setup) return "can't place after setup is complete";
    if (board(dest).figure != Figure::None) return "occupied";
    if (Count(board, L(e.figure == board.player)) == 2) return "can't place anymore";
    return nullopt;
}

optional<string_view> Place(Board& board, Coord dest) {
    auto s = CanPlace(board, dest);
    if (s != nullopt) return s;
    board(dest).figure = board.player;
    return nullopt;
}

optional<string_view> CanMove(const Board& board, Coord src, Coord dest) {
    if (!IsValid(src) || !IsValid(dest)) return "invalid coord";
    if (board.setup) return "can't move during setup";
    if (board.moved) return "moved already";
    if (board(src).figure != board.player) return "player doesn't have figure at src";
    if (board(dest).figure != Figure::None) return "dest isn't empty";
    if (board(dest).level - board(src).level > 1) return "dest is too high";
    if (!Nearby(src, dest)) return "src and dest aren't nearby";
    return nullopt;
}

optional<string_view> Move(Board& board, Coord src, Coord dest) {
    auto s = CanMove(board, src, dest);
    if (s != nullopt) return s;
    board(dest).figure = board.player;
    board(src).figure = Figure::None;
    board.moved = dest;
    return nullopt;
}

optional<string_view> CanBuild(const Board& board, Coord dest, bool dome) {
    if (!IsValid(dest)) return "invalid coord";
    if (board.setup) return "can't build during setup";
    if (!board.moved) return "need to move";
    if (board.built) return "already built";
    if (board(dest).figure != Figure::None) return "can only build on empty space";
    if (dome && board(dest).level != 3) return "dome can only be built on level 3";
    if (!dome && board(dest).level == 3) return "floor can only be built on levels 0, 1 and 2";
    if (!Nearby(*board.moved, dest)) return "can only build near moved figure";
    return nullopt;
}

optional<string_view> Build(Board& board, Coord dest, bool dome) {
    auto s = CanBuild(board, dest, dome);
    if (s != nullopt) return s;
    if (dome) board(dest).figure = Figure::Dome;
    if (!dome) board(dest).level += 1;
    board.built = true;
    return nullopt;
}

optional<string_view> CanExecute(const Board& board, const Step& step) {
    return std::visit(
        overloaded{[&](NextStep a) { return CanNext(board); }, [&](PlaceStep a) { return CanPlace(board, a.dest); },
                   [&](MoveStep a) { return CanMove(board, a.src, a.dest); },
                   [&](BuildStep a) { return CanBuild(board, a.dest, a.dome); }},
        step);
}

optional<string_view> Execute(Board& board, const Step& step) {
    return std::visit(
        overloaded{[&](NextStep a) { return Next(board); }, [&](PlaceStep a) { return Place(board, a.dest); },
                   [&](MoveStep a) { return Move(board, a.src, a.dest); },
                   [&](BuildStep a) { return Build(board, a.dest, a.dome); }},
        step);
}

bool IsMoveBlocked(const Board& board) {
    for (Coord a : kAll) {
        if (board(a).figure == board.player) {
            for (Coord b : kAll) {
                if (CanMove(board, a, b) == nullopt) return false;
            }
        }
    }
    return true;
}

bool IsBuildBlocked(const Board& board) {
    if (!board.moved) return false;
    for (Coord b : kAll) {
        if (CanBuild(board, b, false) == nullopt || CanBuild(board, b, true) == nullopt) return false;
    }
    return true;
}

bool OnThirdLevel(const Board& board) { return Any(kAll, L(board(e).figure == board.player && board(e).level == 3)); }

Figure Winner(const Board& board) {
    if (board.setup) return Figure::None;
    if (!board.moved && IsMoveBlocked(board)) return Other(board.player);
    if (OnThirdLevel(board)) return board.player;
    if (!board.built && IsBuildBlocked(board)) return Other(board.player);
    return Figure::None;
}

// Computer interface
// ==================

std::mt19937_64& Random() {
    static atomic<size_t> seed = 0;
    thread_local bool initialized = false;
    thread_local std::mt19937_64 random;
    if (!initialized) {
        random = std::mt19937_64(seed++);
        initialized = true;
    }
    return random;
}

int RandomInt(int count, std::mt19937_64& random) { return std::uniform_int_distribution<int>(0, count - 1)(random); }

double RandomDouble(std::mt19937_64& random) { return std::uniform_real_distribution<double>(0, 1)(random); }

int ChooseWeighted(double u, cspan<double> weights) {
    FOR(i, weights.size()) {
      if (u < weights[i]) return i;
      u -= weights[i];
    }
    return weights.size() - 1;
}

struct ReservoirSampler {
    std::uniform_real_distribution<double> dis;
    size_t count = 0;

    ReservoirSampler() : dis(0.0, 1.0) {}

    template <typename Random>
    bool operator()(Random& random) {
        return dis(random) * ++count <= 1.0;
    }
};

// Random is used in case of ties.
template<typename T>
int Argmax(std::mt19937_64& random, cspan<T> values) {
    int best_i = -1;
    T best_value;
    ReservoirSampler sampler;

    FOR(i, values.size()) {
        if (sampler.count == 0 || values[i] > best_value) {
            sampler.count = 1;
            best_i = i;
            best_value = values[i];
            continue;
        }
        if (values[i] == best_value && sampler(Random())) best_i = i;
    }
    return best_i;
}

Coord MyRandomFigure(const Board& board, std::mt19937_64& random) {
    Coord out;
    ReservoirSampler sampler;
    for (Coord e : kAll)
        if (board(e).figure == board.player && sampler(random)) out = e;
    return out;
}

Step RandomStep(const Board& board) {
    std::mt19937_64& random = Random();
    if (board.setup) {
        int c = RandomInt(1 + 8, random);
        if (c == 0) return NextStep{};
        return PlaceStep{Coord::Random(random)};
    }

    int c = RandomInt(1 + 2 * 4, random);
    if (c == 0) return NextStep{};
    if (c <= 4) return MoveStep{MyRandomFigure(board, random), Coord::Random(random)};
    return BuildStep{Coord::Random(random), bool(RandomInt(2, random))};
}

template <typename Visitor>
bool Visit(const Board& board, const Step& step, const Visitor& visit) {
    Board my_board = board;
    if (Execute(my_board, step) != nullopt) return true;
    return visit(my_board, step);
}

#define VISIT(A) \
    if (!Visit(board, A, visit)) return false;

template <typename Visitor>
bool AllValidSteps(const Board& board, const Visitor& visit) {
    VISIT(NextStep{});
    if (board.setup) {
        for (Coord e : kAll) VISIT(PlaceStep{e});
        return true;
    }
    for (Coord e : kAll) {
        if (board(e).figure == board.player) {
            for (Coord d : kAll)
                if (d != e) VISIT((MoveStep{e, d}));
        }
    }
    for (bool dome : {false, true})
        for (Coord e : kAll) VISIT((BuildStep{e, dome}));
    return true;
}

bool IsEndOfTurn(const Board& board) {
    bool next = false;
    bool other = false;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        if (std::holds_alternative<NextStep>(step))
            next = true;
        else
            other = true;
        return !other;
    });
    return next && !other;
}

template <typename Visitor>
bool AllValidStepSequences(const Board& board, Action& temp, const Visitor& visit) {
    return AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        temp.push_back(step);
        auto winner = Winner(new_board);
        if (std::holds_alternative<NextStep>(step) || winner != Figure::None) {
            if (!visit(temp, new_board, winner)) return false;
        } else {
            if (!AllValidStepSequences(new_board, temp, visit)) return false;
        }
        temp.pop_back();
        return true;
    });
}

bool IsValid(const Board& board, const Step& step) {
    Board my_board = board;
    return Execute(my_board, step) == nullopt;
}

Step AutoRandom(const Board& board) {
    while (true) {
        Step step = RandomStep(board);
        if (IsValid(board, step)) return step;
    }
}

Step AutoGreedy(const Board& board) {
    auto& random = Random();
    Action temp;
    Step choice;
    ReservoirSampler sampler;
    AllValidStepSequences(board, temp, [&](const Action& action, const Board& new_board, Figure winner) {
        // Print(action);
        if (winner == board.player) {
            choice = action[0];
            sampler.count = 1;
            return false;
        }
        if (winner == Figure::None && sampler(random)) choice = action[0];
        return true;
    });
    Check(sampler.count > 0);
    return choice;
}

const double Pow10[] = {1, 10, 100, 1000};

double ClimbRank(Figure player, const Board& board) {
    double rank = 0;
    for (Coord e : kAll) {
        if (board(e).figure == player) rank += Pow10[int(board(e).level)];
        if (board(e).figure == Other(player)) rank -= Pow10[int(board(e).level)];
    }
    return rank;
}

// Heuristics to use:
// - points for every worker which is not blocked
// - number of free cells around every worker (if they are not too tall)
// - number of squares that my workers can reach in 1 step that opponent's workers can't reach in 2 action? (breaks for Artemis and Hermes)
// - number of non-domed and non-occupied levels within 1 step from my workers
// - number of non-domed and non-occupied levels within 2 action from my workers (that were not included in above)

Step AutoClimber(const Board& board) {
    auto& random = Random();
    Action temp;
    Step choice;
    ReservoirSampler sampler;
    double best_rank = -1e100;
    AllValidStepSequences(board, temp, [&](const Action& action, const Board& new_board, Figure winner) {
        // Print(action);
        if (winner == board.player) {
            choice = action[0];
            best_rank = 1e100;
            sampler.count = 1;
            return false;
        }
        if (winner != Figure::None) return true;

        double rank = ClimbRank(board.player, new_board);
        if (rank == best_rank && sampler(random)) choice = action[0];
        if (rank > best_rank) {
            best_rank = rank;
            choice = action[0];
            sampler.count = 1;
        }
        return true;
    });
    Check(sampler.count > 0);
    return choice;
}

size_t Rollout(Figure player, Board board) {
    auto& random = Random();
    while (true) {
        // Select step uniformly out of all possible action!
        Step random_step;
        ReservoirSampler sampler;
        AllValidSteps(board, [&](const Board& new_board, const Step& step) {
            if (sampler(random)) random_step = step;
            return true;
        });

        Check(Execute(board, random_step) == nullopt);
        auto w = Winner(board);
        if (w != Figure::None) return (w == player) ? 1 : 0;
    }
}

struct Node {
    Step step;
    Board board;  // board state post-step
    Figure winner;
    size_t w = 0;  // number of wins
    size_t n = 0;  // total number of rollouts (w/n is win ratio)
    vector<std::unique_ptr<Node>> children;
};

size_t ChooseChild(size_t N, const vector<std::unique_ptr<Node>>& children) {
    size_t best_i = 0;
    double best_ucb1 = 0;
    for (size_t i = 0; i < children.size(); i++) {
        const auto& child = children[i];
        double ucb1 = (child->n == 0) ? std::numeric_limits<double>::infinity()
                                      : (child->w / child->n + 2 * sqrt(log(N) / child->n));
        if (ucb1 > best_ucb1) {
            best_ucb1 = ucb1;
            best_i = i;
        }
    }
    return best_i;
}

void Expand(const Board& board, vector<std::unique_ptr<Node>>& out) {
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        auto node = std::make_unique<Node>();
        node->step = step;
        node->board = new_board;
        node->winner = Winner(new_board);
        out.push_back(std::move(node));
        return true;
    });
}

size_t MCTS_Iteration(size_t N, Figure player, std::unique_ptr<Node>& node) {
    if (node->winner != Figure::None) {
        size_t e = (player == node->winner) ? 1 : 0;
        node->w += e;
        node->n += 1;
        return e;
    }

    if (node->children.size() > 0) {
        size_t i = ChooseChild(N, node->children);
        size_t e = MCTS_Iteration(N, player, node->children[i]);
        node->w += e;
        node->n += 1;
        return e;
    }

    if (node->n == 0) {
        size_t e = Rollout(player, node->board);
        node->w += e;
        node->n += 1;
        return e;
    }

    Expand(node->board, node->children);
    Check(node->children.size() > 0);

    auto& child = node->children[RandomInt(node->children.size(), Random())];
    size_t e = MCTS_Iteration(N, player, child);
    node->w += e;
    node->n += 1;
    return e;
}

optional<Step> TrivialStep(const Board& board) {
    optional<Step> trivial_step;
    size_t count = 0;
    Action temp;
    bool done = false;
    AllValidStepSequences(board, temp, [&](const Action& action, const Board& new_board, Figure winner) {
        Check(!done);
        if (winner == board.player) {
            trivial_step = action[0];
            done = true;
            return false;
        }
        if (winner != Figure::None) return true;
        // TODO check if opponent can win in one sequence!
        trivial_step = (count == 0) ? optional{action[0]} : nullopt;
        count += 1;
        return true;
    });
    return trivial_step;
}

Step AutoMCTS(const Board& board, const size_t iterations, const bool trivial = false) {
    if (trivial) {
        auto a = TrivialStep(board);
        if (a) return *a;
    }

    vector<std::unique_ptr<Node>> children;
    Expand(board, children);
    if (children.size() == 1) return children[0]->step;

    for (size_t i = 0; i < iterations; i++) {
        size_t ci = ChooseChild(i, children);
        MCTS_Iteration(i, board.player, children[ci]);
    }

    double best_v = 0;
    size_t best_i = 0;
    for (size_t i = 0; i < children.size(); i++) {
        double v = double(children[i]->w) / children[i]->n;
        if (v > best_v) {
            best_v = v;
            best_i = i;
        }
    }
    return children[best_i]->step;
}

double MiniMax(Figure player, const Board& board, int depth) {
    if (depth == 0) return ClimbRank(player, board);

    double best_m = (board.player == player) ? -1e100 : 1e100;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        double m = MiniMax(player, new_board, depth - 1);
        if (board.player == player && m > best_m) best_m = m;
        if (board.player != player && m < best_m) best_m = m;
        return true;
    });
    return best_m;
}

Step AutoMiniMax(const Board& board, const int depth) {
    Step best_step;
    size_t count = 0;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        best_step = step;
        count += 1;
        return count < 2;
    });
    if (count == 1) return best_step;

    auto& random = Random();
    double best_score = -1e100;
    ReservoirSampler sampler;
    AllValidSteps(board, [&](const Board& new_board, const Step& step) {
        double m = MiniMax(board.player, new_board, depth);
        if (m == best_score && sampler(random)) best_step = step;
        if (m > best_score) {
            count = 1;
            best_step = step;
            best_score = m;
        }
        return true;
    });
    return best_step;
}

// Human interface
// ===============

constexpr int Width = 1000, Height = 1000;

vector<Board> g_history;
Board g_board_copy;
optional<Coord> g_selected;

Board g_board;

void Play(optional<string_view> status) {
    if (status) {
        fmt::print("{}\n", *status);
        return;
    }

    g_history.push_back(g_board_copy);
    g_selected = nullopt;
    auto w = Winner(g_board);
    if (w != Figure::None) {
        fmt::print("Player {} wins!\n", PlayerName(w));
        return;
    }

    if (!IsEndOfTurn(g_board)) return;

    g_history.push_back(g_board);
    Check(Next(g_board) == nullopt);
    w = Winner(g_board);
    if (w != Figure::None) {
        fmt::print("Player {} wins!\n", PlayerName(w));
        return;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int step, int mods) {
    if (step == GLFW_PRESS && key == GLFW_KEY_ESCAPE && mods == GLFW_MOD_SHIFT) {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_SPACE) {
        g_board_copy = g_board;
        Play(Next(g_board));
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_U) {
        if (g_history.size() > 0) {
            g_board = g_history.back();
            g_history.pop_back();
        }
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_1) {
        g_board_copy = g_board;
        Step step = AutoRandom(g_board);
        Play(Execute(g_board, step));
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_2) {
        g_board_copy = g_board;
        Step step = AutoGreedy(g_board);
        Play(Execute(g_board, step));
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_3) {
        g_board_copy = g_board;
        Step step = AutoClimber(g_board);
        Play(Execute(g_board, step));
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_4) {
        g_board_copy = g_board;
        Step step = AutoMCTS(g_board, 10000);
        Play(Execute(g_board, step));
    }
    if (step == GLFW_PRESS && key == GLFW_KEY_5) {
        g_board_copy = g_board;
        Step step = AutoMiniMax(g_board, 12);
        Play(Execute(g_board, step));
    }
}

void OnClick(Coord dest, bool left, bool shift) {
    g_board_copy = g_board;
    if (!left) {
        Play(Build(g_board, dest, shift));
    } else if (g_board.setup) {
        Play(Place(g_board, dest));
    } else if (g_board(dest).figure == g_board.player) {
        g_selected = dest;
    } else if (g_selected) {
        Play(Move(g_board, *g_selected, dest));
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int step, int mods) {
    if (step == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        int mouse_x = floor(xpos - 100) / 160;
        int mouse_y = floor(Height - ypos - 100) / 160;
        if (mouse_x >= 0 && mouse_x < 5 && mouse_y >= 0 && mouse_y < 5)
            OnClick({mouse_x, mouse_y}, button == GLFW_MOUSE_BUTTON_LEFT, mods == GLFW_MOD_SHIFT);
    }
}

void scroll_callback(GLFWwindow* window, double x, double y) {}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }

struct View : public VertexBuffer_vec2_rgba {
    Shader shader;
    Uniform_mat4 transform;
    FontRenderer font_renderer;
    Font mono;
    glm::mat4 ortho;

    View()
        : VertexBuffer_vec2_rgba(25),
          shader(R"END(
    		#version 330 core
    		layout (location = 0) in vec2 pos;
    		layout (location = 1) in vec4 rgba;
    		out vec4 pixel_rgba;
    		uniform mat4 transform;

            void main() {
    		    vec4 p = transform * vec4(pos, 0.0, 1.0);
		        gl_Position = p;
                pixel_rgba = rgba;
	    	}

		    #version 330 core
    		in vec4 pixel_rgba;
    		out vec4 color;

    		void main() {
    		    color = pixel_rgba;
    		}
        	)END"),
          transform("transform"),
          font_renderer(1000, 1000),
          mono("santorini/JetBrainsMono-Medium.ttf", 48, &font_renderer) {}
};

void Render(const Board& board, View& view) {
    glUseProgram(view.shader);
    view.bind();
    view.transform = view.ortho;

    // Frame
    for (int i = 0; i <= 5; i++) {
        const uint64_t color = 0xFF808080;
        view.add({100, 100 + i * 160}, color);
        view.add({100 + 800, 100 + i * 160}, color);

        view.add({100 + i * 160, 100}, color);
        view.add({100 + i * 160, 100 + 800}, color);
    }
    view.draw(GL_LINES);

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            const double s = 160;
            const double px = 100 + x * s + s / 2;
            const double py = 100 + y * s + s / 2;
            const Coord c(x, y);
            const auto cell = board(c);

            if (g_selected && c == *g_selected) {
                double a = 80;
                const uint64_t color = 0xFF00FFFF;
                view.add({px - a, py - a}, color);
                view.add({px + a, py - a}, color);
                view.add({px + a, py + a}, color);
                view.add({px - a, py + a}, color);
                view.draw(GL_LINE_LOOP);
            }

            // Towers
            if (cell.level >= 1) {
                double a = 70;
                const uint64_t color = 0xFFFFFFFF;
                view.add({px - a, py - a}, color);
                view.add({px + a, py - a}, color);
                view.add({px + a, py + a}, color);
                view.add({px - a, py + a}, color);
                view.draw(GL_LINE_LOOP);
            }
            if (cell.level >= 2) {
                const uint64_t color = 0xFFFFFFFF;
                double a = 60;
                view.add({px - a, py - a}, color);
                view.add({px + a, py - a}, color);
                view.add({px + a, py + a}, color);
                view.add({px - a, py + a}, color);
                view.draw(GL_LINE_LOOP);
            }
            if (cell.level >= 3) {
                const uint64_t color = 0xFFFFFFFF;
                double a = 50;
                view.add({px - a, py - a}, color);
                view.add({px + a, py - a}, color);
                view.add({px + a, py + a}, color);
                view.add({px - a, py + a}, color);
                view.draw(GL_LINE_LOOP);
            }

            // Domes
            if (cell.figure == Figure::Dome) {
                const uint64_t color = 0xFFFF0000;
                double a = 40;
                for (int i = 0; i < 16; i++) {
                    double k = i * 2 * M_PI / 16;
                    view.add({px + cos(k) * a, py + sin(k) * a}, color);
                }
                view.draw(GL_TRIANGLE_FAN);
            }

            // Player pieces
            if (cell.figure == Figure::Player1 || cell.figure == Figure::Player2) {
                const uint64_t color = (cell.figure == Figure::Player1) ? 0xFF00FFFF : 0xFF0000FF;
                double a = 40;
                view.add({px - a, py - a}, color);
                view.add({px + a, py - a}, color);
                view.add({px + a, py + a}, color);
                view.add({px - a, py + a}, color);
                view.draw(GL_TRIANGLE_FAN);
            }
        }
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    view.mono.moveTo(10, 980);
    view.mono.m_scale = 13.0 / 48;
    view.mono.m_color = Color("FFFFFF");
    view.mono.render(fmt::format("Player {}", PlayerName(board.player)));
    if (board.moved) view.mono.render(" Moved");
    if (board.built) view.mono.render(" Built");
    glDisable(GL_BLEND);
}

// Benchmarks and training
// -----------------------

using Policy = std::function<Step(const Board&)>;

Figure Battle(const Policy& policy_a, const Policy& policy_b) {
    Board board;
    while (true) {
        auto w = Winner(board);
        if (w != Figure::None) return w;
        const Policy& policy = (board.player == Figure::Player1) ? policy_a : policy_b;
        auto s = Execute(board, policy(board));
        if (s != nullopt) {
            fmt::print("faul\n");
            return Other(board.player);
        }
    }
}

const std::unordered_map<string_view, Policy> g_policies = {
    {"random", AutoRandom},
    {"greedy", AutoGreedy},
    {"climber", AutoClimber},
    {"mcts100", [](const auto& e) { return AutoMCTS(e, 100); }},
    {"mcts100t", [](const auto& e) { return AutoMCTS(e, 100, true); }},
    {"mcts200", [](const auto& e) { return AutoMCTS(e, 200); }},
    {"mcts400", [](const auto& e) { return AutoMCTS(e, 400); }},
    {"minimax6", [](const auto& e) { return AutoMiniMax(e, 6); }},
    {"minimax9", [](const auto& e) { return AutoMiniMax(e, 9); }},
    {"minimax12", [](const auto& e) { return AutoMiniMax(e, 12); }}};

void AutoBattle(int count, string_view name_a, string_view name_b) {
    const Policy& policy_a = g_policies.at(name_a);
    const Policy& policy_b = g_policies.at(name_b);
    atomic<int> wins_a = 0, wins_b = 0;

    atomic<bool> stop = false;
    thread monitor([&]() {
        string message;
        while (!stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            FOR(j, message.size()) cout << '\r';
            message = fmt::format("{} {} : {} {}", name_a, wins_a, wins_b, name_b);
            cout << message;
            cout.flush();
        }
        cout << '\n';
    });

    parallel_for(count, [&](size_t i) {
        if (Battle(policy_a, policy_b) == Figure::Player1)
            wins_a += 1;
        else
            wins_b += 1;
        if (Battle(policy_b, policy_a) == Figure::Player1)
            wins_b += 1;
        else
            wins_a += 1;
    });

    stop = true;
    monitor.join();
}

struct Agent {
    virtual Action Play(const Board& board) = 0;
};

struct SARS {
    Board state;
    Board step; // board state after agent's turn, but before opponent's turn (step + state)
    float reward;
    optional<Board> next_state; // board state after opponent's turn (or nullopt if game ends after agent's or opponent's turns)
};

class ReplayBuffer {
public:
    ReplayBuffer(int capacity) { buffer.reserve(capacity); }

    vector<SARS> Sample(int count) const {
        std::mt19937_64& random = Random();
        if (buffer.size() < count) count = buffer.size();

        vector<int> selected(count);
        for (int i = 0; i < count; i++) {
            while (true) {
                int e = RandomInt(buffer.size(), random);
                if (contains(cspan<int>(selected.data(), i), e)) continue;
                selected[i] = e;
                break;
            }
        }

        vector<SARS> sample(count);
        for (int i = 0; i < count; i++) sample[i] = buffer[selected[i]];
        return sample;
    }

    void Add(const SARS& e) {
        if (buffer.size() == buffer.capacity()) buffer.pop_front();
        buffer.push_back(e);
    }

private:
    mt::array_deque<SARS> buffer;
};

// network which learns Board -> float mapping
class BoardNet {
public:
    BoardNet() {
        in = Data({5, 5, 7}) << "input";
        ref = Data({1}) << "reference";
        auto w_init = make_shared<NormalInit>(0.01, Random());

        auto x = in;
        x = Conv2D(x, ConvType::Same, dim4(16, 1, 1, 7, 'i', 'w', 'h', 'c'), w_init);
        //b = Conv2D(x, ConvType::Same, dim4(16, 3, 3, 7, 'i', 'w', 'h', 'c'), w_init);
        //x = Concat(a, b);
        x = BatchNorm(x, 1e-8);
        x = Relu(x);
        x = Conv2D(x, ConvType::Valid, dim4(1, 5, 5, 32, 'i', 'w', 'h', 'c'), w_init);
        x = BatchNorm(x, 1e-8);
        out = Logistic(x, 15);

        loss = MeanSquareError(ref, out);
        model = Model({loss});
        model.Print();
        model.optimizer = std::make_shared<Optimizer>();
        model.optimizer->alpha = env("alpha", 0.01);
    }

    const vector<float>& Eval(const vector<MiniBoard>& batch) {
        model.SetBatchSize(batch.size()); // TODO this might be slow if different
        FOR(i, batch.size()) ToTensor(batch[i], in->v.slice(i));
        model.Forward(/*training*/false);
        return out->v.vdata();
    }

    void Train(const vector<pair<MiniBoard, float>>& batch) {
        model.SetBatchSize(batch.size());
        FOR(i, batch.size()) {
            ToTensor(batch[i].first, in->v.slice(i));
            ref->v[i] = batch[i].second;
        }
        model.Forward(/*training*/true);
        model.Backward(loss);
    }

private:
    Diff in, ref, out, loss;
    Model model;
};

// TODO Use Prioritized Replay Buffer (select SARS transitions with the worst fit).
// TODO Use Double DQN: q1 and q2 (instead of q_policy and q_train), and flip them randomly.
// TODO Can we use Dueling DQN here (with variable number of action)?
class DQNAgent : public Agent {
public:
    const double boltzmann_tau = 1;
    const float gamma = 0.999;

    bool _explore = true;

    // Inference only. No policy improvement or sample collection.
    Action Play(const Board& board) override {
        vector<MiniBoard> all_boards;
        vector<Action> all_action;

        AllValidStepSequences(board, _temp, [&](Action& action, const Board& new_board, auto winner_ignored) {
            all_boards << static_cast<MiniBoard>(new_board);
            all_action << action;
            return true;
        });

        if (all_boards.empty()) return {};
        return all_action[ChooseStep(_q_policy.Eval(all_boards))];
    }

    // TODO How are transitions collected?
    // TODO How is training invoked?
    // TODO This training can happen in PyTorch python script on carbide, while network could be loaded into C++ for inference.
    void ImprovePolicy(const Figure ego) {
        const Figure opponent = Other(ego);
        const int sample_size = 16;
        const int K = 1000;

        FOR(i, K) {
            _xy.clear();
            // TODO Collect all new boards from double nested loop, and do all evals at once.
            // TODO Parallelize this loop.
            for (const auto& [state, step, reward, next_state] : _replay_buffer.Sample(sample_size)) {
                float max_q = 0;
                if (next_state.has_value()) {
                    max_q = -1;
                    AllValidStepSequences(next_state.value(), _temp, [&](Action& action, const Board& new_board, auto winner) {
                        if (winner == ego) {
                          max_q = 1;
                        } else if (winner != opponent) {
                          max_q = std::max(max_q, _q_policy.Eval({new_board})[0]);
                        }
                        return true;
                    });
                    // If no possible move then game is lost and max_q == -1.
                }
                _xy.emplace_back(static_cast<MiniBoard>(step), reward + gamma * max_q);
            }
            _q_train.Train(_xy); // with Huber loss (quadratic for small errors, linear for large errors)
        }
        // TODO evaluate loss against the entire replay_buffer

        // TODO Copy weights from q_train to q_policy.
    }

private:
    int ChooseStep(const vector<float>& q_values) {
        if (!_explore) return Argmax(Random(), cspan<float>(q_values));

        // Subtract average to reduce chance of overflow.
        // TODO Subtracting max value might be better.
        double avg = Sum(q_values, 0.0) / q_values.size();
        vector<double> exp_q_values;
        for (float v : q_values) exp_q_values << exp((v - avg) * boltzmann_tau);
        const double u = RandomInt(q_values.size(), Random()) * Sum(exp_q_values, 0.0);
        return ChooseWeighted(u, exp_q_values);
    }

    ReplayBuffer _replay_buffer;
    BoardNet _q_train, _q_policy;

    // Cached vectors to avoid reallocation.
    vector<pair<MiniBoard, float>> _xy;
    Action _temp;
};

struct RandomAgent : public Agent {
    Action Play(const Board& board) override {
        Action action;
        Board my_board = board;
        while (true) {
            Step step = AutoRandom(my_board);
            action << step;
            Execute(my_board, step);
            if (std::holds_alternative<NextStep>(step) || Winner(my_board) != Figure::None) break;
        }
        return action;
    }
};

struct GreedyAgent : public Agent {
    Action Play(const Board& board) override {
        Action action;
        Board my_board = board;
        while (true) {
            Step step = AutoGreedy(my_board);
            action << step;
            Execute(my_board, step);
            if (std::holds_alternative<NextStep>(step) || Winner(my_board) != Figure::None) break;
        }
        return action;
    }
};

// Plan:
// - value function is based on the entire turn (1 or more action of same player)
// - play both as player1 and player2
// - keep accumulating experience (and save it to file)
// - play games in parallel
// - after every 1000 games train new network (from scratch?) (using all previous experience)
// - if new network plays worse after 1000 games -> discard it
// - port to carbide!

// Network architecture:
// - board is 5x5x7 bits without any extra bits
// : layer0
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer1
// - Concat(in)
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer2
// - Concat(in, layer1)
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer3
// - Concat(in, layer1, layer2)
// - Conv2D 3x3 with 10 channels
// - BatchNorm
// - Relu
// : layer4
// - Concat(in, layer1, layer2, layer3)
// - Valid Conv2D 3x3 with 10 channels -> 3x3xM
// - BatchNorm
// - Relu
// : layer5
// - Flatten
// - Dense
// - BatchNorm
// - Relu
// - Dense
// - Sigmoid

Figure PlayOneGame(Board board, Agent& agent_a, Agent& agent_b, Values& values, vector<Board>& history) {
    history.clear();

    Figure w = Winner(board);
    while (w == Figure::None) {
        Agent& agent = (board.player == Figure::Player1) ? static_cast<Agent&>(agent_a) : static_cast<Agent&>(agent_b);

        bool eot = false;
        for (Step step : agent.Play(board)) {
            if (eot) Fail("invalid step after end of turn");
            auto s = Execute(board, step);
            if (s != nullopt) Fail(fmt::format("invalid step {}", s));
            eot = std::holds_alternative<NextStep>(step) || Winner(board) != Figure::None;
        }
        if (!eot) Fail("player didn't end turn");

        history << board;
        w = Winner(board);
    }

    Figure p = Figure::Player1;
    for (const Board& board : history) {
        values.Add(board, Score(w));
        p = board.player;
    }
    return w;
}

Score PlayManyGames(Agent& agent_a, Agent& agent_b, const size_t tasks, const size_t task_size, Values& values) {
    atomic<size_t> next = 0, completed = 0;
    Score score;
    mutex score_mutex;

    parallel([&]() {
        Values local_values;
        Score local_score;
        vector<Board> history;
        for (size_t task = next++; task < tasks; task = next++) {
            local_values.Clear();
            FOR(i, task_size) {
                auto w = PlayOneGame(Board(), agent_a, agent_b, local_values, history);
                if (w == Figure::Player1) local_score.p1 += 1;
                if (w == Figure::Player2) local_score.p2 += 1;
            }
            values.Merge(local_values);
            auto c = ++completed;
            if (c % 50 == 0) {
                std::unique_lock lock(g_cout_mutex);
                fmt::print("started {}, finished {}\n", next.load(), c);
            }
        }
        std::unique_lock lock(score_mutex);
        score += local_score;
    });
    return score;
}

void Learn(Values& values) {
    RandomAgent agent_a;
    //MonteCarloAgent agent_a(0.01, true, 100);

    //MonteCarloAgent agent_b(0.01, false, 1000);
    RandomAgent agent_b;
    //GreedyAgent agent_b;

    std::ofstream stats("stats.txt");

    Timestamp begin;
    PlayManyGames(agent_a, agent_b, 100, 2000, values);
    Timestamp end;
    fmt::print("elapsed {}, states {}\n", begin.elapsed_s(end), values.Size());

    // self-play
/*    double ratio = 0.5;
    Values values;
    vector<Board> history;
    size_t db = 0;
    while (true) {
        auto winner = PlayOneGame(agent_a, agent_b, history);

        Figure p = Figure::Player1;
        for (const Board& board : history) {
            values.Add(board, (winner == p) ? 1 : 0, 1);
            p = board.player;
        }

        constexpr double q = 0.0025;
        ratio = ratio * (1 - q) + (winner == Figure::Player1 ? 1 : 0) * q;
        if ((game + 1) % 10000 == 0) println("Game %s (wins %.4f, values %s)", game + 1, ratio, values.Size());

        if ((game + 1) % 100000 == 0) {
            println("Saving after %s games", game + 1);
            values.Export(format("db_%s.values", ++db));
        }
    }*/
}

void Browse(const Values& values) {
    vector<Board> stack = { Board() };
    while (true) {
        fmt::print("\n");
        Render(stack.back());
        Action temp;
        vector<Board> options;
        AllValidStepSequences(stack.back(), temp, [&](Action& action, const Board& new_board, auto winner) {
            if (values.Lookup(new_board).has_value()) options.push_back(new_board);
            return true;
        });

        int id = 0;
        for (const Board& b : options) {
            auto score = values.Lookup(b);
            column_section(10, 15);
            fmt::print("[{}]", id);
            if (score) fmt::print("{} {}", score->p1, score->p2); else fmt::print("\n");
            id += 1;
            Render(b);
        }
        end_column_section();

        while (true) {
            fmt::print("> ");
            std::cin >> id;
            if (-1 <= id && id < int(options.size())) break;
        }

        if (id == -1 && stack.size() > 1) stack.pop_back();
        if (id >= 0) stack.push_back(options[id]);
    }
}

int main(int argc, char** argv) {
    InitSegvHandler();

    if (argc > 1 && argv[1] == "learn"s) {
        Values values;
        Learn(values);
        values.Export("random_vs_random.values");
        return 0;
    }

    if (argc > 1 && argv[1] == "browse"s) {
        Values values("random_vs_random.values");
        Browse(values);
        return 0;
    }

    if (argc > 1 && argv[1] == "combo"s) {
        Values values;
        Learn(values);
        Browse(values);
        return 0;
    }

    if (argc > 1) {
        AutoBattle(100, "climber", "minimax12");
        AutoBattle(1000, "mcts100", "mcts100t");
        AutoBattle(100, "minimax6", "minimax9");
        AutoBattle(100, "climber", "minimax9");
        AutoBattle(100, "climber", "minimax6");
        AutoBattle(100, "climber", "mcts100");
        AutoBattle(100, "climber", "mcts200");
        AutoBattle(100, "climber", "mcts400");
        return 0;
    }

    auto window = CreateWindow({.width = Width, .height = Height, .resizeable = false});
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glViewport(0, 0, Width, Height);

    glClearColor(0.0, 0.0, 0.0, 1.0);

    View view;
    view.ortho = glm::ortho(0.0, double(Width), 0.0, double(Height));

    RunEventLoop(window, [&]() {
        glClear(GL_COLOR_BUFFER_BIT);
        Render(g_board, view);
    });
    return 0;
}
