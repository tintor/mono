def library(name, srcs=[], deps=[]):
  native.cc_library(
    name = name,
    hdrs = [name + ".h"],
    srcs = srcs,
    deps = deps,
  )
  
  native.cc_test(
    name = name + "_test",
    srcs = [name + "_test.cc"],
    deps = [":" + name, "//:catch"],
    args = ["-d=yes"],
  )
