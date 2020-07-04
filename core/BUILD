package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

library(
    name = "auto",
)

library(
    name = "numeric",
    deps = [":auto"],
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
    name = "algorithm",
    deps = [":span"],
)

cc_library(
    name = "vector",
    hdrs = ["vector.h"],
    deps = ["@boost//:container"],
)

library(
    name = "dim4",
    deps = [":numeric", ":span", ":check", "@fmt"],
)

library(
    name = "tensor",
    deps = [":dim4"],
)

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

library(
    name = "util",
    deps = [":span"],
)

library(
    name = "thread",
    deps = [":numeric"],
)

library(
    name = "diff",
    deps = [":random", ":algorithm", ":tensor", ":util", ":check"],
)

library(name = "array_deque", deps = [":numeric"])

library(name = "range", deps = [":span", "@fmt"])
