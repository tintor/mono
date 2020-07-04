def library(name, hdrs=[], srcs=[], deps=[], test_deps=[]):
  native.cc_library(
    name = name,
    hdrs = [name + ".h"] + hdrs,
    srcs = srcs,
    deps = deps,
  )
  
  native.cc_test(
    name = name + "_test",
    srcs = [name + "_test.cc"],
    deps = test_deps + [":" + name, "//:catch"],
    args = ["-d=yes"],
  )
