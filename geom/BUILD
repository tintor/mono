package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

library(name = "vector", deps = ["//core:numeric", "//core:align_alloc"], test_deps = ["@fmt"])
library(name = "segment", deps = [":vector"])
cc_library(name = "edges", hdrs = ["edges.h"], deps = [":segment", "//core:each", "//core:span"])
library(name = "triangle", deps = [":segment", ":edges"])
