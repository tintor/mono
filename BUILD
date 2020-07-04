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
