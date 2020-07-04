package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

library(name = "auto")
library(name = "numeric", deps = [":auto"])

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

library(name = "algorithm", deps = [":span"])
cc_library(name = "vector", hdrs = ["vector.h"], deps = ["@boost//:container"])

library(
    name = "dim4",
    deps = [":numeric", ":span", ":check", "@fmt"],
)

library(name = "tensor", deps = [":dim4"])

cc_library(
    name = "random",
    hdrs = ["random.h"],
    deps = [":span"],
)

cc_library(
    name = "timestamp",
    hdrs = ["timestamp.h"],
    srcs = ["timestamp.cc"],
    deps = [":algorithm", ":auto", ":numeric"],
)

cc_library(
    name = "align_alloc",
    hdrs = ["align_alloc.h"],
)

library(name = "string_util", deps = [":span", ":algorithm"])
library(name = "util", srcs = ["util.cc"], deps = [":numeric", ":span", ":string_util"])
library(name = "thread", deps = [":numeric"])
library(name = "range", deps = [":span", "@fmt"])
library(name = "array_deque", deps = [":numeric"])

library(
    name = "diff",
    srcs = ["diff.cc"],
    deps = [":random", ":range", ":algorithm", ":tensor", ":util", ":check"],
)

cc_library(
    name = "optimizer",
    hdrs = ["optimizer.h"],
    srcs = ["optimizer.cc"],
    deps = [":diff", ":numeric", ":timestamp"],
)

library(
    name = "model",
    srcs = ["model.cc"],
    deps = [":timestamp", ":optimizer", ":vector"],
    test_deps = [":thread", ":range", ":vector"],
)
