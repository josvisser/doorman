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
#include <iostream>

#include "cxx/interval_release_queue.h"
#include "gtest/gtest.h"

namespace {

// A simple timer class which returns elapsed time.
class Timer {
  using Clock = std::chrono::steady_clock;

 public:
  Timer() { Restart(); }

  void Restart() { start_ = Clock::now(); }

  template <typename T>
  int Get() {
    return std::chrono::duration_cast<T>(Clock::now() - start_).count();
  }

 private:
  Clock::time_point start_;
};

class IntervalReleaseQueueTest : public ::testing::Test {
 protected:
  // Executes a simple test of the interval release queue using a single thread
  // and a blocking Wait() call. It returns the number of milliseconds the test
  // took.
  int SingleThreadedTestUsingBlockingWait(int limit, int msec, int num_loops) {
    q_.SetRateLimit(limit, msec);

    Timer t;

    while (num_loops-- > 0) {
      q_.Wait();
    }

    return t.Get<std::chrono::milliseconds>();
  }

  // Executes a test using a single thread but using a non-blocking wait. Waits
  // for the last operation to finish and then returns the number of
  // milliseconds the test took.
  int SingleThreadedTestUsingNonBlockingWait(int limit, int msec,
                                             int num_loops) {
    int n = num_loops;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    std::condition_variable cond;

    q_.SetRateLimit(limit, msec);

    Timer t;

    while (num_loops-- > 0) {
      q_.Wait([&] {
        std::lock_guard<std::mutex> lock(mutex);
        --n;
      });
    }

    cond.wait(lock, [&]() -> bool { return n == 0; });

    return t.Get<std::chrono::milliseconds>();
  }

  // Used in all the tests.
  doorman::IntervalReleaseQueue q_;
};

// This macro really should exist in Google Test.
#define EXPECT_BETWEEN(lo, hi, actual) \
  {                                    \
    auto x = (actual);                 \
    EXPECT_LE((lo), x);                \
    EXPECT_GE((hi), x);                \
  }

// Tests whether the blocking wait actually blocks.
TEST_F(IntervalReleaseQueueTest, BlockingWaitBlocks) {
  // Sets the queue to 1 qps.
  q_.SetRateLimit(1, 1000);

  Timer t;

  // The first wait should take 0 seconds (between zero and one millisecond).
  q_.Wait();
  EXPECT_BETWEEN(0, 1, t.Get<std::chrono::milliseconds>());

  // The next wait should take slightly less than 1 second.
  t.Restart();
  q_.Wait();
  EXPECT_BETWEEN(990, 1100, t.Get<std::chrono::milliseconds>());
}

// Tests basic operation at rate of 1/10msec (100 qps).
TEST_F(IntervalReleaseQueueTest, BasicOperationBlockingWait1) {
  EXPECT_BETWEEN(990, 1050, SingleThreadedTestUsingBlockingWait(1, 10, 100));
}

// Tests basic operation at rate of 1/10msec (100 qps).
TEST_F(IntervalReleaseQueueTest, BasicOperationBlockingWait2) {
  EXPECT_BETWEEN(10000, 11000, SingleThreadedTestUsingBlockingWait(1, 10, 1000));
}

// Tests basic operation at rate of 100/1sec (100 qps).
TEST_F(IntervalReleaseQueueTest, BasicOperationBlockingWait3) {
  EXPECT_BETWEEN(0, 10, SingleThreadedTestUsingBlockingWait(100, 1000, 100));
}

// Rate of 1/10msec (100 qps). 101 operations should take 1 second.
TEST_F(IntervalReleaseQueueTest, ExactRatePlusOneBlocksBlockingWait) {
  EXPECT_BETWEEN(1000, 1100, SingleThreadedTestUsingBlockingWait(1, 10, 101));
}

// The nonblocking wait should return immediately.
TEST_F(IntervalReleaseQueueTest, NonBlockingWaitDoesNotBlock) {
  // Sets the queue to 1 qps.
  q_.SetRateLimit(1, 1000);

  Timer t;

  // The first wait should take 0 seconds (between zero and one millisecond).
  q_.Wait([] {});
  EXPECT_BETWEEN(0, 1, t.Get<std::chrono::milliseconds>());

  // The next wait should also take 0 seconds (between zero and one).
  t.Restart();
  q_.Wait([] {});
  EXPECT_BETWEEN(0, 1, t.Get<std::chrono::milliseconds>());
  std::cout << "Done\n";
}

// Rate of 1/10msec (100 qps). 1000 operations should take 10 seconds.
TEST_F(IntervalReleaseQueueTest, BasicOperationNonBlockingWait) {
  EXPECT_BETWEEN(10, 12, SingleThreadedTestUsingNonBlockingWait(1, 10, 1000));
}

// Negative limit means queue is not rate limited.
TEST_F(IntervalReleaseQueueTest, NegativeLimitMeansQueueNotRateLimited) {
  q_.SetRateLimit(-1, 0);

  Timer t;

  for (int i = 0; i < 5000; ++i) {
    q_.Wait();
  }

  EXPECT_EQ(0, t.Get<std::chrono::seconds>());
}

// Rate of zero means the queue is blocked.
TEST_F(IntervalReleaseQueueTest, ZeroLimitMeansQueueIsBlocked) {
  std::atomic_bool has_run(false);

  q_.SetRateLimit(0, 0);
  q_.Wait([&] { has_run = true; });
  sleep(5);
  EXPECT_FALSE(has_run);
  q_.SetRateLimit(1, 10);

  while (!has_run) {
    sleep(1);
  }
}

// A worker thread for the multithreaded test. Increments a counter in a tight
// loop until told to stop.
class Worker {
 public:
  Worker(doorman::IntervalReleaseQueue* q, std::atomic_bool* stop,
         std::atomic_int* num)
      : q_(q), stop_(stop), num_(num) {
    thread_.reset(new std::thread(&Worker::_Run, this));
  }

  ~Worker() {
    // Ensures that the embedded thread has stopped.
    thread_->join();
  }

 private:
  void _Run() {
    while (!stop_) {
      q_->Wait();
      ++num_;
    }
  }

  doorman::IntervalReleaseQueue* q_;
  std::atomic_bool* stop_;
  std::atomic_int* num_;
  std::unique_ptr<std::thread> thread_;
};

// Multithreaded test. This test has a high flakiness index because it depends
// on execution timings.
TEST_F(IntervalReleaseQueueTest, MultiThreadedTest) {
  const int kNumThreads = 100;
  Worker* workers[kNumThreads];
  std::atomic_bool stop(false);
  std::atomic_int num(0);

  q_.SetRateLimit(0, 0);

  for (int i = 0; i < kNumThreads; ++i) {
    workers[i] = new Worker(&q_, &stop, &num);
  }

  sleep(1);
  q_.SetRateLimit(10, 100);
  sleep(10);
  stop = true;

  for (auto worker : workers) {
    delete worker;
  }

  EXPECT_BETWEEN(900, 1100, (int)num);
}

}  // namespace
