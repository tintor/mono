package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_library(
    name = "model",
    hdrs = ["santorini.cc", "action.h", "board.h", "cell.h", "coord.h"],
)

cc_binary(
    name = "santorini",
    srcs = ["santorini.cc"],
    deps = [":model", "//view:font", "//view:vertex_buffer", "//view:window", "//core:callstack", "//core:vector", "//core:check", "//core:column", "//core:thread", "//core:model", "//core:util", "@fmt", "@absl//absl/container:flat_hash_map"],
)

cc_test(
    name = "santorini_test",
    srcs = ["santorini_test.cc"],
    deps = ["//:catch"],
)
