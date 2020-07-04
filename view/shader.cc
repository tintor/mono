#include <array>
#include <string_view>
using namespace std;

#include <fmt/core.h>

#include "core/auto.h"
#include "view/shader.h"

Shader::Shader(string_view source) {
    size_t n = source.rfind("#version ");
    string_view vert = source.substr(0, n);
    string_view frag = source.substr(n);

    uint vertex = compile(GL_VERTEX_SHADER, vert);
    uint fragment = compile(GL_FRAGMENT_SHADER, frag);
    ON_SCOPE_EXIT(glDeleteShader(vertex));
    ON_SCOPE_EXIT(glDeleteShader(fragment));

    m_id = glCreateProgram();
    glAttachShader(m_id, vertex);
    glAttachShader(m_id, fragment);
    glLinkProgram(m_id);

    int success;
    glGetProgramiv(m_id, GL_LINK_STATUS, &success);
    if (!success) {
        array<char, 1024> infoLog;
        glGetProgramInfoLog(m_id, infoLog.size(), nullptr, infoLog.data());
        fmt::print("PROGRAM LINKING ERROR:\n");
        fmt::print("%s\n", std::string(infoLog.data()));
        exit(0);
    }
    use();
}

uint Shader::compile(int type, string_view source) {
    const char* ptr = source.data();
    int length = source.size();

    uint shader = glCreateShader(type);
    glShaderSource(shader, 1, &ptr, &length);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        array<char, 1024> infoLog;
        glGetShaderInfoLog(shader, infoLog.size(), nullptr, infoLog.data());
        fmt::print("%s SHADER COMPILATION ERROR:\n", (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT");
        fmt::print("%s\n", string(infoLog.data()));
        exit(0);
    }
    return shader;
}

static string_view gluEnumString(GLenum e) {
    switch (e) {
        case GL_NO_ERROR:
            return "NO_ERROR";
        case GL_INVALID_ENUM:
            return "INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";

        case GL_FLOAT_VEC2:
            return "FLOAT_VEC2";
        case GL_FLOAT_VEC3:
            return "FLOAT_VEC3";
        case GL_FLOAT_VEC4:
            return "FLOAT_VEC4";

        case GL_FLOAT_MAT2:
            return "FLOAT_VEC2";
        case GL_FLOAT_MAT3:
            return "FLOAT_MAT3";
        case GL_FLOAT_MAT4:
            return "FLOAT_MAT4";
    }
    return "(unknown)";
}

Uniform::Uniform(string_view name, int expectedType) {
    int shader;
    glGetIntegerv(GL_CURRENT_PROGRAM, &shader);

    array<char, 128> buffer;
    if (name.size() >= buffer.size()) {
        fmt::print("uniform name too long [%s]\n", name);
        exit(0);
    }
    memcpy(buffer.data(), name.data(), name.size());
    buffer[name.size()] = 0;

    m_location = glGetUniformLocation(shader, buffer.data());
    if (m_location == -1) {
        fmt::print("bad uniform name [%s]\n", name);
        exit(0);
    }

    int uniforms;
    glGetProgramiv(shader, GL_ACTIVE_UNIFORMS, &uniforms);
    for (int i = 0; i < uniforms; i++) {
        array<char, 128> buffer;
        int length;
        int size;
        GLenum type;
        glGetActiveUniform(shader, i, buffer.size(), &length, &size, &type, buffer.data());
        if (name == string_view(buffer.data(), length)) {
            if (type != expectedType) {
                fmt::print("uniform [%s] expected type [%s] instead of [%s]\n", name, gluEnumString(expectedType),
                      gluEnumString(type));
                exit(0);
            }
            break;
        }
    }
}
