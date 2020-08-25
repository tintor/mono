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

http_archive(
    name = "glm",
    urls = ["https://github.com/g-truc/glm/archive/0.9.9.8.tar.gz"],
    build_file = "@//:glm.BUILD"
)

new_git_repository(
    name = "glfw",
    build_file = "@//:glfw.BUILD",
    remote = "https://github.com/glfw/glfw.git",
    branch = "3.3-stable",
)

# BOOST
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "com_github_nelhage_rules_boost",
    commit = "1e3a69bf2d5cd10c34b74f066054cd335d033d71",
    remote = "https://github.com/nelhage/rules_boost",
    shallow_since = "1591047380 -0700",
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()
