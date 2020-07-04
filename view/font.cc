#include <ft2build.h>
#include "view/font.h"
#include "view/opengl.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include FT_FREETYPE_H

#include "core/auto.h"
#include "core/format.h"
#include "view/shader.h"

FontRenderer::FontRenderer(double width, double height)
    : shader(R"END(
		#version 330 core
		layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
		out vec2 TexCoords;

		uniform mat4 projection;

		void main() {
			gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
			TexCoords = vertex.zw;
		}

		#version 330 core
		in vec2 TexCoords;
		out vec4 color;

		uniform sampler2D text;
		uniform vec3 textColor;

		void main() {
			vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
			color = vec4(textColor, 1.0) * sampled;
		}
	)END") {
    glm::mat4 projection = glm::ortho(0.0, width, 0.0, height);
    shader.use();
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    textColorLocation = glGetUniformLocation(shader, "textColor");

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * CharsPerBuffer, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

Font::Font(string_view name, int resolution, FontRenderer *renderer) : m_color("FFFFFF") {
    m_renderer = renderer;

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        exit(0);
    }
    ON_SCOPE_EXIT(FT_Done_FreeType(ft));

    // Load font as face
    // TODO do search in both: /System/Library/Fonts and /Library/Fonts with
    // various extensions
    const char *fmt = name.find('.') != string_view::npos ? "%s" : "/Library/Fonts/%s.ttf";
    FT_Face face;
    if (FT_New_Face(ft, format(fmt, name).c_str(), 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font: " << format(fmt, name) << std::endl;
        exit(0);
    }
    ON_SCOPE_EXIT(FT_Done_Face(face));

    // Set size to load glyphs as
    FT_Set_Pixel_Sizes(face, 0, resolution);

    ON_SCOPE_EXIT(glBindTexture(GL_TEXTURE_2D, 0));
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    int texture_width = 1;
    int texture_height = 0;

    for (int c = 0; c < m_characters.size(); c++) {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            printf("ERROR::FREETYTPE: Failed to load Glyph '%c' (%d)\n", c, c);
            exit(0);
        }

        // TODO check if rendering will be faster if letters are placed
        // vertically instead of horisontaly in the texture (to keep all texels
        // for single char close)
        int w = face->glyph->bitmap.width;
        int h = face->glyph->bitmap.rows;
        if (w > 0 && h > 0) {
            texture_height = std::max(texture_height, 2 + h);
            texture_width += w + 1;
        }
    }

    uint8_t *data = new uint8_t[texture_width * texture_height];
    memset(data, 0, texture_width * texture_height);
    ON_SCOPE_EXIT(delete[] data);
    int texture_offset = 1;
    float tw = texture_width;
    float th = texture_height;

    for (int c = 0; c < m_characters.size(); c++) {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            printf("ERROR::FREETYTPE: Failed to load Glyph '%c' (%d)\n", c, c);
            exit(0);
        }

        int size_x = face->glyph->bitmap.width;
        int size_y = face->glyph->bitmap.rows;

        m_characters[c] = {texture_offset / tw, (texture_offset + size_x) / tw, 1 / th, (1 + size_y) / th,

                           size_x, size_y, face->glyph->bitmap_left, face->glyph->bitmap_top,
                           // Advance is number of 1/64 pixels.
                           // Bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of
                           // 1/64th pixels by 64 to get amount of pixels))
                           int(face->glyph->advance.x >> 6)};

        int w = face->glyph->bitmap.width;
        int h = face->glyph->bitmap.rows;
        uint8_t *buffer = face->glyph->bitmap.buffer;
        if (w > 0 && h > 0) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    data[(y + 1) * texture_width + texture_offset + x] = buffer[y * w + x];
                }
            }
            texture_offset += w + 1;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texture_width, texture_height, 0, GL_RED, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    m_max_size_y = 0;
    for (const Character &ch : m_characters) {
        m_max_size_y = std::max(m_max_size_y, ch.size_y);
    }
}

Font::~Font() {
    // TODO delete textures
}

void Font::render(string_view text, double scale, Color color) {
    m_renderer->shader.use();
    glUniform3f(m_renderer->textColorLocation, color.r, color.g, color.b);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_renderer->VAO);

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glBindBuffer(GL_ARRAY_BUFFER, m_renderer->VBO);

    int offset = 0;
    for (char c : text) {
        if (c == '\n') {
            m_x = m_left;
            m_y -= m_max_size_y * scale * 1.2;
            continue;
        }

        Character ch = m_characters[c];
        if (ch.size_x == 0 || ch.size_y == 0) {
            m_x += ch.advance * scale;
            continue;
        }

        GLfloat xpos = m_x + ch.bearing_x * scale;
        GLfloat ypos = m_y - (int(ch.size_y) - int(ch.bearing_y)) * scale;

        GLfloat w = ch.size_x * scale;
        GLfloat h = ch.size_y * scale;

        // Update VBO for each character
        float vertices[6 * 4] = {
            xpos, ypos + h, ch.u0, ch.v0, xpos,     ypos, ch.u0, ch.v1, xpos + w, ypos,     ch.u1, ch.v1,

            xpos, ypos + h, ch.u0, ch.v0, xpos + w, ypos, ch.u1, ch.v1, xpos + w, ypos + h, ch.u1, ch.v0,
        };

        m_x += ch.advance * scale;

        memcpy(m_renderer->vertices + offset * 6 * 4, vertices, 6 * 4 * sizeof(float));
        offset += 1;

        if (offset == CharsPerBuffer) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 6 * 4 * offset, m_renderer->vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6 * offset);
            offset = 0;
        }
    }
    if (offset > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 6 * 4 * offset, m_renderer->vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6 * offset);
        offset = 0;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
