package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_library(
    name = "model",
    hdrs = ["action.h", "board.h", "cell.h", "coord.h", "execute.h", "reservoir_sampler.h", "enumerator.h"],
    srcs = ["execute.cc", "board.cc"],
    deps = ["@absl//absl/container:flat_hash_map", "//core:numeric", "//core:fmt", "//core:column", "//core:algorithm", "//core:tensor"],
)

cc_library(
    name = "greedy",
    hdrs = ["greedy.h"],
    srcs = ["greedy.cc"],
    deps = [":model", "//core:random"],
)

cc_library(
    name = "minimax",
    hdrs = ["minimax.h", "minimax_generic.h"],
    srcs = ["minimax.cc"],
    deps = [":greedy"],
)

cc_library(
    name = "mcts",
    hdrs = ["mcts.h"],
    srcs = ["mcts.cc"],
    deps = [":model", "//core:random"],
)

cc_library(
    name = "ui",
    srcs = ["ui.cc"],
    deps = ["//view:font", "//view:vertex_buffer", "//view:window", "//core:random", ":model", ":minimax", ":mcts", ":greedy"],
)

cc_binary(
    name = "santorini",
    srcs = ["santorini.cc"],
    data = ["JetBrainsMono-Medium.ttf"],
    deps = [":model", ":minimax", ":mcts", ":ui",
            "//core:array_deque", "//core:callstack", "//core:vector", "//core:check", "//core:column", "//core:thread", "//core:model", "//core:string"],
)

cc_test(
    name = "santorini_test",
    srcs = ["santorini_test.cc"],
    deps = ["//:catch"],
)
