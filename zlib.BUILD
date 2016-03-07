cc_library(
  name = "main",
  srcs = glob([
    "src/*.cc",
    "src/*.h"
  ]),
  hdrs = glob([
    "include/**/*.h",
  ]),
  copts = ["-Iexternal/gtest/include"],
  linkopts = ["-pthread"],
  visibility = ["//visibility:public"],
)
