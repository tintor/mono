package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_library(name = "state", hdrs = ["state.h"], deps = ["//core:array_bool"])
cc_library(name = "cell", hdrs = ["cell.h"], deps = ["//core:numeric"])
cc_library(name = "util", hdrs = ["util.h"], deps = [":cell", ":state"])
cc_library(name = "code", hdrs = ["code.h"])
cc_library(name = "emoji", hdrs = ["emoji.h"])
cc_library(name = "hungarian", hdrs = ["hungarian.h"])

cc_library(
    name = "frozen",
    hdrs = ["frozen.h"],
    deps = ["//core:thread", "@absl//absl/container:flat_hash_map"],
)

cc_library(
    name = "level",
    hdrs = ["level.h"],
    srcs = ["level.cc"],
    deps = [":code", ":util", "//core:range", "//core:matrix", "//core:file", "//core:array_deque", "//core:exception", "//core:small_bfs"],
)

cc_library(
    name = "corrals",
    hdrs = ["corrals.h"],
    srcs = ["corrals.cc"],
    deps = [":level", ":frozen", ":util"],
)

cc_library(
    name = "solver",
    hdrs = ["solver.h"],
    srcs = ["solver.cc"],
    deps = [":hungarian", ":corrals", ":level", ":state", "//core:timestamp", "//core:thread", "//core:array_deque", "//core:bits", "//core:range", "//core:small_bfs", "//core:string", "@fmt", "@absl//absl/container:flat_hash_set"],
)

cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [":solver", "@fmt", "//core:vector", "//core:auto", "//core:bits", "//core:range", "//core:string", "//core:thread", "//core:timestamp"],
    data = glob(["levels/**"]),
)
