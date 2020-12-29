#include <iostream>

#include "core/fmt.h"
#include "core/random.h"
#include "core/timestamp.h"

#include "santorini/execute.h"
#include "santorini/enumerator.h"
#include "santorini/greedy.h"
#include "santorini/minimax.h"
#include "santorini/mcts.h"

#include "view/font.h"
#include "view/glm.h"
#include "view/shader.h"
#include "view/vertex_buffer.h"
#include "view/window.h"

using namespace std;

constexpr int Width = 1000, Height = 1000;

vector<Board> g_history;
Board g_board_copy;
optional<Coord> g_selected;

Board g_board;

static bool IsEndOfTurn(const Board& board) {
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

static void Play(optional<string_view> status) {
    if (status) {
        print("{}\n", *status);
        return;
    }

    g_history.push_back(g_board_copy);
    g_selected = nullopt;
    auto w = Winner(g_board);
    if (w != Figure::None) {
        print("Player {} wins!\n", PlayerName(w));
        return;
    }

    if (!IsEndOfTurn(g_board)) return;

    g_history.push_back(g_board);
    Check(Next(g_board) == nullopt);
    w = Winner(g_board);
    if (w != Figure::None) {
        print("Player {} wins!\n", PlayerName(w));
        return;
    }
}

Action g_ai_action;

static void key_callback(GLFWwindow* window, int key, int scancode, int step, int mods) {
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
    if (g_ai_action.empty()) {
        if (step == GLFW_PRESS && key == GLFW_KEY_1) {
            g_board_copy = g_board;
            g_ai_action = AutoGreedy(g_board);
        }
        if (step == GLFW_PRESS && key == GLFW_KEY_2) {
            g_board_copy = g_board;
            g_ai_action = AutoClimber(g_board);
        }
        if (step == GLFW_PRESS && key == GLFW_KEY_3) {
            g_board_copy = g_board;
            Weights weights{.mass1 = 0.2, .mass2 = 0.4, .mass3 = 0.8};
            g_ai_action = AutoClimber(g_board, weights);
        }
        if (step == GLFW_PRESS && key == GLFW_KEY_4) {
            g_board_copy = g_board;
            g_ai_action = QuickStart(MCTS(3200, true))(g_board);
        }
        if (step == GLFW_PRESS && key == GLFW_KEY_5) {
            g_board_copy = g_board;
            g_ai_action = QuickStart(MiniMax(2, true))(g_board);
        }
        if (step == GLFW_PRESS && key == GLFW_KEY_6) {
            g_board_copy = g_board;
            g_ai_action = QuickStart(MiniMax(4, true))(g_board);
        }
    }
}

static void OnClick(Coord dest, bool left, bool shift) {
    g_board_copy = g_board;
    if (!left) {
        Play(Build(g_board, dest, shift));
    } else if (g_board.phase == Phase::PlaceWorker) {
        Play(Place(g_board, dest));
    } else if (g_board(dest).figure == g_board.player) {
        g_selected = dest;
    } else if (g_selected) {
        Play(Move(g_board, *g_selected, dest));
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int step, int mods) {
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

static void Render(const Board& board, View& view) {
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
    if (board.phase == Phase::GameOver) {
        view.mono.render(format("Player {} wins!", PlayerName(board.player)));
    } else {
        view.mono.render(format("Player {}", PlayerName(board.player)));
        if (board.moved) view.mono.render(" Moved");
        if (board.build) view.mono.render(" Built");
        view.mono.render(format(" {} vs {}", CardName(board.my_card()), CardName(board.opp_card())));
    }
    glDisable(GL_BLEND);
}

void RunUI(Card card1, Card card2) {
    g_board.card1 = card1;
    g_board.card2 = card2;

    auto window = CreateWindow({.width = Width, .height = Height, .resizeable = false});
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glViewport(0, 0, Width, Height);

    glClearColor(0.0, 0.0, 0.0, 1.0);

    View view;
    view.ortho = glm::ortho(0.0, double(Width), 0.0, double(Height));

    Timestamp last_ai_step;
    RunEventLoop(window, [&]() {
        if (g_ai_action.size() > 0 && last_ai_step.elapsed_ms() > 1000) {
            Play(Execute(g_board, g_ai_action[0]));
            g_ai_action.erase(g_ai_action.begin());
            last_ai_step = Timestamp();
        }
        glClear(GL_COLOR_BUFFER_BIT);
        Render(g_board, view);
    });
}
