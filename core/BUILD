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
    name = "tensor",
    hdrs = ["tensor.h"],
    deps = [":int", ":span", "@fmt"],
)

cc_test(
    name = "tensor_test",
    srcs = ["tensor_test.cc"],
    deps = [":tensor", ":check", "@fmt", "//:catch"],
    args = ["-d=yes"],
)
