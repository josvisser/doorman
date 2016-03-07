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

git_repository(
  name = "grpc",
  remote = "https://github.com/grpc/grpc.git",
  tag = "release-0_13_0",
)

bind(
  name = "grpc_ext",
  actual = "@grpc//:grpc++",
)

git_repository(
  name = "proto3",
  remote = "https://github.com/google/protobuf.git",
  tag = "v3.0.0-beta-2",
)

bind(
  name = "protobuf_clib",
  actual = "@proto3//:protobuf",
)
