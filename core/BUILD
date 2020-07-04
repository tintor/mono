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

cc_library(
    name = "dim4",
    hdrs = ["dim4.h"],
    deps = [":int", ":span", ":check", "@fmt"],
)

cc_test(
    name = "dim4_test",
    srcs = ["dim4_test.cc"],
    deps = [":dim4", "//:catch"],
    args = ["-d=yes"],
)

cc_library(
    name = "tensor",
    hdrs = ["tensor.h"],
    deps = [":dim4"],
)

cc_test(
    name = "tensor_test",
    srcs = ["tensor_test.cc"],
    deps = [":tensor", "//:catch"],
    args = ["-d=yes"],
)
