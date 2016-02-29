cc_library(
  name = "thread_pool",
  srcs = [
    "thread_pool.cc",
  ],
  hdrs = [
    "thread_pool.h",
  ]
)

cc_test(
  name = "thread_pool_test",
  srcs = [
    "thread_pool_test.cc",
  ],
  copts = [
    "-Iexternal/gtest/include",
  ],
  deps = [
    ":thread_pool",
    "@gtest//:main",
  ],
)
