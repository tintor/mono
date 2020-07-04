#pragma once
#include <array>
#include <vector>
#include <mutex>
#include <string_view>

#include "fmt/core.h"
#include "view/opengl.h"

void Error(const char* name);

int gen_buffer(GLenum target, GLsizei size, const void* data);
GLuint load_program(std::string_view name, bool geometry = false);
void load_png_texture(std::string_view filename);

// TODO make it work for vertical monitor setup
const int ConsoleWidth = 131;
const int ConsoleHeight = 40;  // TODO: magic numbers?

class Console {
   public:
    Console();
    virtual void Execute(std::string_view command) { Print("[%s]\n", command); }

    template <typename... Args>
    void Print(std::string_view fmt, Args... args) {
        std::lock_guard lock(_mutex);
        PrintInternal(fmt::format(fmt, args...));
    }

    static bool KeyToChar(int key, int mods, char& ch);
    bool OnKey(int key, int mods);
    // void Render(Text* text, float time);

    bool IsVisible() { return _visible; }
    void Show() { _visible = true; }
    void Hide() { _visible = false; }

    // json_t* save();
    // bool load(json_t* doc);

   private:
    void PrintInternal(std::string_view s);

    bool _visible = false;
    std::mutex _mutex;

    char _output[ConsoleHeight][ConsoleWidth];
    int _last_line = 0;
    int _last_column = 0;

    struct Input {
        std::array<char, ConsoleWidth> buffer;
        int cursor;
    };

    int _current_input = 0;
    std::vector<Input*> _inputs;
};
