#pragma once
#include <core/std.h>
#include <view/color.h>
#include <view/shader.h>

// TODO: ANTI-ALIAS: Valve's signed distance fields:
//       - https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
// TODO: custom window sizes
// TODO: unicode characters
// TODO: x and y positioning: left, center, right
// TODO: 3d text positioning
// TODO: text rotation
// TODO: compute text size

// TODO: markup language to change text style and color
// TODO: text console

constexpr int CharsPerBuffer = 100;

struct FontRenderer {
    uint VAO, VBO;
    Shader shader;
    int textColorLocation;
    float vertices[6 * 4 * CharsPerBuffer];

    FontRenderer(double width, double height);
};

class Font {
   public:
    Font(string_view name, int resolution, FontRenderer* renderer);
    ~Font();
    void render(string_view text, double scale, Color color);

    // range for (x, y) is always (800, 600)
    void moveTo(double x, double y) {
        m_left = x;
        m_x = x;
        m_y = y;
    }

    void render(string_view text, double x, double y, double scale, Color color) {
        moveTo(x, y);
        render(text, scale, color);
    }

    double m_scale;
    Color m_color;

    void render(string_view text) { render(text, m_scale, m_color); }

   private:
    FontRenderer* m_renderer = nullptr;
    int m_max_size_y = 0;

    float m_left = 0;
    float m_x = 0;
    float m_y = 0;

    uint m_texture = 0;

    struct Character {
        float u0;
        float u1;
        float v0;
        float v1;

        int size_x, size_y;        // Size of glyph
        int bearing_x, bearing_y;  // Offset from baseline to left/top of glyph
        int advance;               // Horizontal offset to advance to next glyph
    };

    array<Character, 128> m_characters;
};
