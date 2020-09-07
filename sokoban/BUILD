package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_library(
    name = "level_env",
    hdrs = ["level_env.h"],
    srcs = ["level_env.cc"],
    deps = ["//core:file", "//core:matrix", "//core:exception", "//core:fmt"],
)

library(name = "state", deps = ["//core:array_bool", "//core:exception", "//core:fmt"])
cc_library(name = "cell", hdrs = ["cell.h"], deps = ["//core:numeric"])
cc_library(name = "util", hdrs = ["util.h"], deps = [":cell", ":state"])
cc_library(name = "counters", hdrs = ["counters.h"], deps = ["//core:fmt"])
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
    deps = [":level_env", ":util", "//core:vector", "//core:range", "//core:array_deque", "//core:small_bfs"],
)

cc_library(
    name = "deadlock",
    hdrs = ["deadlock.h"],
    deps = [":level", ":util", ":counters"],
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
    deps = [
        ":hungarian", ":corrals", ":level", ":level_env", ":state", ":deadlock",
        "//core:timestamp", "//core:thread", "//core:array_deque", "//core:bits", "//core:range", "//core:small_bfs", "//core:string",
        "@absl//absl/container:flat_hash_set",
    ],
)

cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        ":solver",
        "//core:vector", "//core:auto", "//core:bits", "//core:range", "//core:string", "//core:thread", "//core:timestamp", "//core:fmt",
        "@boost//:program_options"
    ],
    data = glob(["levels/**"]),
)
