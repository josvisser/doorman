# Copyright 2016 Google, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cc_library(
    name = "thread_pool",
    srcs = [
        "thread_pool.cc",
    ],
    hdrs = [
        "thread_pool.h",
    ],
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
        "//external:gtest_ext",
    ],
)

cc_library(
    name = "executor",
    srcs = [
        "executor.cc",
    ],
    hdrs = [
        "executor.h",
    ],
    deps = [
        ":thread_pool",
    ],
)

cc_test(
    name = "executor_test",
    srcs = [
        "executor_test.cc",
    ],
    copts = [
        "-Iexternal/gtest/include",
    ],
    deps = [
        ":executor",
        "//external:gtest_ext",
    ],
)

cc_library(
    name = "interval_release_queue",
    srcs = [
        "interval_release_queue.cc",
    ],
    hdrs = [
        "interval_release_queue.h",
    ],
    deps = [
        ":executor",
    ],
)

cc_test(
    name = "interval_release_queue_test",
    srcs = [
        "interval_release_queue_test.cc",
    ],
    copts = [
        "-Iexternal/gtest/include",
    ],
    deps = [
        ":interval_release_queue",
        "//external:gtest_ext",
    ],
)

cc_library(
    name = "openssl",
    srcs = glob(["third_party/openssl/lib*.a"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "doorman_client",
    srcs = [
        "doorman_client.cc",
    ],
    hdrs = [
        "doorman_client.h",
    ],
    deps = [
        "//external:grpc_ext",
    ],
)

cc_library(
    name = "dummy",
    visibility = ["//visibility:public"]
)
