#pragma once
#include "core/numeric.h"
#include "view/glm.h"
#include "view/opengl.h"

class Shader {
   public:
    Shader(std::string_view source);
    operator uint() const { return m_id; }

    void use() { glUseProgram(m_id); }

   private:
    uint compile(int type, std::string_view source);

   private:
    uint m_id = 0;
};

class Uniform {
   public:
    Uniform(std::string_view name, int expectedType);

   protected:
    int m_location = -1;
};

#define UNIFORM(TYPE, NAME) Uniform_##TYPE NAME##Uniform(#NAME)

class Uniform_vec2 : public Uniform {
   public:
    Uniform_vec2(std::string_view name) : Uniform(name, GL_FLOAT_VEC2) {}
    void operator=(const vec2& value) { glUniform2fv(m_location, 1, glm::value_ptr(value)); }
};

class Uniform_vec3 : public Uniform {
   public:
    Uniform_vec3(std::string_view name) : Uniform(name, GL_FLOAT_VEC3) {}
    void operator=(const vec3& value) { glUniform3fv(m_location, 1, glm::value_ptr(value)); }
};

class Uniform_vec4 : public Uniform {
   public:
    Uniform_vec4(std::string_view name) : Uniform(name, GL_FLOAT_VEC4) {}
    void operator=(const vec4& value) { glUniform4fv(m_location, 1, glm::value_ptr(value)); }
};

class Uniform_mat2 : public Uniform {
   public:
    Uniform_mat2(std::string_view name) : Uniform(name, GL_FLOAT_MAT2) {}
    void operator=(const mat2& value) { glUniformMatrix2fv(m_location, 1, false, glm::value_ptr(value)); }
};

class Uniform_mat3 : public Uniform {
   public:
    Uniform_mat3(std::string_view name) : Uniform(name, GL_FLOAT_MAT3) {}
    void operator=(const mat3& value) { glUniformMatrix3fv(m_location, 1, false, glm::value_ptr(value)); }
};

class Uniform_mat4 : public Uniform {
   public:
    Uniform_mat4(std::string_view name) : Uniform(name, GL_FLOAT_MAT4) {}
    void operator=(const mat4& value) { glUniformMatrix4fv(m_location, 1, false, glm::value_ptr(value)); }
};
