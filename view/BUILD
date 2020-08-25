package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_library(
    name = "color",
    hdrs = ["color.h"],
)

cc_library(
    name = "glm",
    hdrs = ["glm.h"],
    deps = ["@fmt", "@glm"],
)

cc_library(
    name = "opengl",
    hdrs = ["opengl.h"],
    deps = ["@glfw"],
)

cc_library(
    name = "shader",
    hdrs = ["shader.h"],
    srcs = ["shader.cc"],
    deps = [":glm", ":opengl", "//core:numeric", "@fmt"],
)

cc_library(
    name = "font",
    hdrs = ["font.h"],
    deps = [":color", ":shader"],
    #srcs = ["font.cc"],
    includes = ["/usr/local/Cellar/freetype/2.10.1/include/freetype2"],
)

cc_library(
    name = "vertex_buffer",
    hdrs = ["vertex_buffer.h"],
)

cc_library(
    name = "rendering",
    hdrs = ["rendering.h"],
    srcs = ["rendering.cc"],
    deps = [":opengl", "//:lodepng", "//core:range", "@fmt"],
)

cc_library(
    name = "window",
    hdrs = ["window.h"],
    srcs = ["window.cc"],
    deps = [":opengl"],
)
