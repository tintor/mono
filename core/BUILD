load("//:library.bzl", "library")

cc_library(
    name = "int",
    hdrs = ["int.h"],
)

cc_library(
    name = "check",
    hdrs = ["check.h"],
    srcs = ["check.cc"],
    deps = ["@fmt"],
)

cc_library(
    name = "span",
    hdrs = ["span.h"],
    deps = ["@absl//absl/types:span"],
)

library(
    name = "dim4",
    deps = [":int", ":span", ":check", "@fmt"],
)

library(
    name = "tensor",
    deps = [":dim4"],
)
