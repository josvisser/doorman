// Copyright 2016 Google, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <chrono>

#include "cxx/executor.h"
#include "gtest/gtest.h"

namespace doorman {

// The simplest of all Executor tests...
TEST(ExecutorTest, SimpleExecutorTest) {
  Executor e;
  bool has_run = false;

  e.Schedule([&has_run] { has_run = true; });

  while (!has_run) {
    sleep(1);
  }
}

// Tests scheduling an item in the future.
// Note: This test is flaky because it depends on exact timings. On busy
// systems scheduling might not cooperate with our expectations...
TEST(ExecutorTest, ScheduleAtWorks) {
  Executor e;
  bool has_run = false;

  e.ScheduleAt(Clock::from_time_t(time(0) + 5), [&has_run] { has_run = true; });
  EXPECT_FALSE(has_run);
  sleep(3);
  EXPECT_FALSE(has_run);
  sleep(3);
  EXPECT_TRUE(has_run);
}

// Tests scheduling an item in the future.
// Note: This test is flaky because it depends on exact timings. On busy
// systems scheduling might not cooperate with our expectations...
TEST(ExecutorTest, ScheduleInWorks) {
  Executor e;
  bool has_run = false;

  e.ScheduleIn(std::chrono::seconds(5), [&has_run] { has_run = true; });
  EXPECT_FALSE(has_run);
  sleep(3);
  EXPECT_FALSE(has_run);
  sleep(3);
  EXPECT_TRUE(has_run);
}

}  // namespace doorman
