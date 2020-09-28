package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        "//core:array_deque", "//core:matrix", "//core:vector", "//core:timestamp", "//core:fmt", "//core:callstack",
        "@absl//absl/container:flat_hash_set", "@absl//absl/container:flat_hash_map",
    ],
)
