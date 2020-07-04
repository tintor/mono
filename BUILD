package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "hello",
    srcs = ["hello.cc"],
    deps = ["@fmt", "//core:random", "//core:align_alloc", "//core:vector", "//core:timestamp"],
)

cc_library(
    name = "catch",
    hdrs = ["catch.hpp"],
)

cc_library(
    name = "lodepng",
    hdrs = ["lodepng/lodepng.h"],
    srcs = ["lodepng/lodepng.cc"],
)

cc_library(
    name = "freetype",
)
