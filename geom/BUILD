package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

library(
    name = "vector",
    deps = ["//core:numeric", "//core:align_alloc"],
    test_deps = ["@fmt"])

cc_library(
    name = "aabb",
    hdrs = ["aabb.h"],
    deps = [":vector"],
)

library(
    name = "segment",
    srcs = ["segment.cc"],
    deps = [":vector", ":aabb", "//core:exception", "//core:span"],
    test_deps = ["@fmt", "//core:check", "//core:string", "//core:vector"])

cc_library(
    name = "edges",
    hdrs = ["edges.h"],
    deps = [":segment", "//core:each", "//core:span"])

library(
    name = "triangle",
    deps = [":segment", ":edges"])
