package(default_visibility = ["//visibility:public"])
load("//:library.bzl", "library")

library(name = "vector", deps = ["//core:numeric", "//core:align_alloc"], test_deps = ["@fmt"])
library(name = "segment", deps = [":vector"])
