new_git_repository(
  name = "gtest",
  build_file = "gtest.BUILD",
  remote = "https://github.com/google/googletest.git",
  tag = "release-1.7.0",
)

bind(
    name = "gtest_ext",
    actual = "@gtest//:main",
)
