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
//
#include "cxx/thread_pool.h"
#include "gtest/gtest.h"

namespace doorman {

// Tests the most basic functionality of the thread pool: Creating a worker
// thread, executing a callback, and shutting down the pool.
TEST(ThreadPoolTest, SimpleThreadPoolTest) {
  bool has_run = false;
  ThreadPool pool(0, 1);

  pool.Schedule([&has_run] { has_run = true; });

  while (!has_run) {
    sleep(1);
  }
}

// Tests scheduling a large number of tasks over a limited number of threads.
TEST(ThreadPoolTest, OverSubscriptionTest) {
  std::atomic_int n(0);
  ThreadPool pool(0, 2);

  for (int i = 0; i < 1000; ++i) {
    pool.Schedule([&n] { ++n; });
  }

  while (n < 1000) {
    sleep(1);
  }
}

// Tests that the thread pool grows and shrinks.
TEST(ThreadPoolTest, GrowsAndShrinks) {
  std::atomic_bool please_stop(false);
  std::atomic_int n1(0);
  std::atomic_int n2(0);
  ThreadPool pool(5, 100);

  for (int i = 0; i < 200; ++i) {
    // This loop ensures that the first 100 threads that we schedule have
    // started before we schedule the next one.
    while (i < 100 && i != n1) {
      usleep(100000);
    }

    pool.Schedule([&please_stop, &n1, &n2] {
      ++n1;
      while (!please_stop) {
        sleep(1);
      }
      ++n2;
    });
  }
  std::cout << "Step 0\n";

  // This means that 100 threads are executing, and the thread pool did not
  // grow beyond that.
  EXPECT_EQ(100, pool.GetStats().current_size);
  EXPECT_EQ(100, n1);

  // Make the threads stop.
  please_stop = true;

  // Waits for all threads to exit.
  while (n2 < 200) {
    sleep(1);
  }

  // All threads must be done.
  std::cout << "Step 2\n";
  EXPECT_EQ(200, n2);

  // The thread pool must have shrunk.
  EXPECT_EQ(5, pool.GetStats().current_size);
  EXPECT_EQ(95, pool.GetStats().num_grow_events);
  EXPECT_EQ(95, pool.GetStats().num_shrink_events);
}

}  // namespace doorman
