package(default_visibility = ["//visibility:public"])

include_files = [
    "include/GLFW/glfw3.h",
    "include/GLFW/glfw3native.h",
]

cc_library(
    name = "glfw",
    hdrs = include_files,
    includes = ["include"],
    linkopts = ["-framework OpenGl -L/usr/local/lib -lglfw"],
)
