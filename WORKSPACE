load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

# abseil-cpp
git_repository(
    name = "absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    branch = "master",
)

http_archive(
    name = "rules_cc",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/262ebec3c2296296526740db4aefce68c80de7fa.zip"],
    strip_prefix = "rules_cc-262ebec3c2296296526740db4aefce68c80de7fa",
    sha256 = "9a446e9dd9c1bb180c86977a8dc1e9e659550ae732ae58bd2e8fd51e15b2c91d"
)

# googletest
http_archive(
    name = "googletest",
    urls = ["https://github.com/google/googletest/archive/011959aafddcd30611003de96cfd8d7a7685c700.zip"],
    strip_prefix = "googletest-011959aafddcd30611003de96cfd8d7a7685c700",
)

# {fmt} "6.2.1"
new_git_repository(
    name = "fmt",
    build_file = "@//:fmt.BUILD",
    remote = "https://github.com/fmtlib/fmt.git",
    commit = "19bd751020a1f3c3363b2eb67a039852f139a8d3",
    shallow_since = "1589040800 -0700",
)

new_git_repository(
    name = "glm",
    remote = "https://github.com/g-truc/glm.git",
    commit = "658d8960d081e0c9c312d49758c7ef919371b428",
    build_file = "@//:glm.BUILD",
)

new_git_repository(
    name = "glfw",
    build_file = "@//:glfw.BUILD",
    remote = "https://github.com/glfw/glfw.git",
    branch = "3.3-stable",
)

# C++ Thread Pool Library
new_git_repository(
    name = "ctpl",
    build_file = "@//:ctpl.BUILD",
    remote = "https://github.com/vit-vit/ctpl.git",
    branch = "master",
)

# BOOST
git_repository(
    name = "com_github_nelhage_rules_boost",
    branch = "master",
    remote = "https://github.com/nelhage/rules_boost",
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

new_local_repository(
    name = "freetype2",
    path = "/opt/X11/include/freetype2",
    build_file = "@//:freetype2.BUILD",
)
