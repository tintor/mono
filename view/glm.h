#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <fmt/format.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>

using glm::dvec2;
using glm::dvec3;
using glm::dvec4;

using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::mat2;
using glm::mat3;
using glm::mat4;

namespace glm {
/*inline void format_e(string& s, string_view spec, dvec2 v) {
    ::format_e(s, spec, v.x);
    s += ' ';
    ::format_e(s, spec, v.y);
}

inline void format_e(string& s, string_view spec, dvec3 v) {
    ::format_e(s, spec, v.x);
    s += ' ';
    ::format_e(s, spec, v.y);
    s += ' ';
    ::format_e(s, spec, v.z);
}*/
}  // namespace glm
