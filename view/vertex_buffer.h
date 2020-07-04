#pragma once

#include <core/std.h>
#include <view/glm.h>
#include <view/opengl.h>

class VertexBuffer_double2 {
   public:
    VertexBuffer_double2(uint count) {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(double2) * count, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_DOUBLE, false, sizeof(double2), 0);
    }

    void write(cspan<double2> vertices) {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(double2), vertices.data());
    }

    void draw(uint type, cspan<double2> vertices) {
        write(vertices);
        glDrawArrays(type, 0, vertices.size());
    }

    void bind() { glBindVertexArray(m_vao); }

   private:
    uint m_vbo, m_vao;
};

class VertexBuffer_vec2_rgba {
   public:
    struct Vertex {
        vec2 pos;
        uint64_t color;
    };

    vector<Vertex> data;

    VertexBuffer_vec2_rgba(uint count) {
        data.reserve(count);

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * count, nullptr, GL_DYNAMIC_DRAW);

        Vertex* v = nullptr;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Vertex), &v->pos);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, true, sizeof(Vertex), &v->color);
    }

    void add(vec2 pos, uint64_t color) { data.push_back(Vertex{pos, color}); }

    void add(double2 pos, uint64_t color) { data.push_back(Vertex{vec2(pos.x, pos.y), color}); }

    void draw(uint type) {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, data.size() * sizeof(Vertex), data.data());
        glDrawArrays(type, 0, data.size());
        data.clear();
    }

    void bind() { glBindVertexArray(m_vao); }

   private:
    uint m_vbo, m_vao;
};

class VertexBuffer_vec2 {
   public:
    struct Vertex {
        vec2 pos;
    };

    vector<Vertex> data;

    VertexBuffer_vec2(uint count) {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * count, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(vec2), 0);
    }

    void write(cspan<vec2> vertices) {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(vec2), vertices.data());
    }

    void draw(uint type, cspan<vec2> vertices) {
        write(vertices);
        glDrawArrays(type, 0, vertices.size());
    }

    void add(vec2 pos) { data.push_back(Vertex{pos}); }

    void draw(uint type) {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, data.size() * sizeof(Vertex), data.data());
        glDrawArrays(type, 0, data.size());
        data.clear();
    }

    void bind() { glBindVertexArray(m_vao); }

   private:
    uint m_vbo, m_vao;
};
