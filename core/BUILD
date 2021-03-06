package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

cc_library(name = "fmt", hdrs = ["fmt.h"], deps = ["@fmt"])

cc_library(name = "symbol", hdrs = ["symbol.h"])

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

cc_library(
    name = "column",
    hdrs = ["column.h"],
    srcs = ["column.cc"],
    deps = [":numeric"],
)

library(name = "algorithm", deps = [":span"])
cc_library(name = "vector", hdrs = ["vector.h"], deps = ["@boost//:container"])
cc_library(name = "bits_util", hdrs = ["bits_util.h"])
library(name = "bits", hdrs = ["murmur3.h"], deps = [":numeric", ":bits_util"])
cc_library(name = "array_bool", hdrs = ["array_bool.h"], deps = ["bits"])

library(name = "string", srcs = ["string.cc"], deps = [":span", ":algorithm", ":numeric"])
library(name = "callstack", srcs = ["callstack.cc"], deps = [":numeric", ":string", ":auto"])
cc_library(name = "exception", hdrs = ["exception.h"], deps = [":callstack", ":string", "@fmt"])
cc_library(name = "matrix", hdrs = ["matrix.h"], deps = [":numeric", ":exception"])
cc_library(name = "random", hdrs = ["random.h"], deps = [":span"])

cc_library(
    name = "timestamp",
    hdrs = ["timestamp.h"],
    srcs = ["timestamp.cc"],
    deps = [":algorithm", ":auto", ":numeric"],
)

cc_library(name = "align_alloc", hdrs = ["align_alloc.h"])

library(name = "thread", deps = [":numeric"])
library(name = "range", deps = [":span", "@fmt"])
library(name = "array_deque", deps = [], test_deps = ["@fmt"])
cc_library(name = "each", hdrs = ["each.h"])
cc_library(name = "small_bfs", hdrs = ["small_bfs.h"], deps = [":each", ":numeric"])
cc_library(name = "file", hdrs = ["file.h"], srcs = ["file.cc"], deps = [":each", ":exception", "@boost//:interprocess"])

library(name = "zip", deps = [":span"])
library(name = "sparse_matrix")

# neural
library(name = "dim4", deps = [":numeric", ":span", ":check", "@fmt"])
library(name = "tensor", deps = [":dim4"])

library(
    name = "diff",
    srcs = ["diff.cc"],
    deps = [":random", ":range", ":algorithm", ":tensor", ":string", ":check"],
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
