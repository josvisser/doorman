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

#ifndef YOUTUBE_DOORMAN_CXX_EXECUTOR_H_
#define YOUTUBE_DOORMAN_CXX_EXECUTOR_H_

#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <mutex>

#include "cxx/thread_pool.h"

namespace doorman {

// Executor is an interface to a background system that runs works in threads
// in a thread pool.
class Executor {
 public:
  // Creates an Executor with a default (potentially large) thread pool.
  Executor() : Executor(std::numeric_limits<int>::max()) {}

  // Creates an Executor with a thread pool of the specified maximum size.
  // Note: The thread pool must contain at least 1 thread!
  Executor(int max_size);

  ~Executor();

  // Schedules a new piece of work on the thread pool. The work gets scheduled
  // immediately.
  void Schedule(std::function<void()> callback) {
    thread_pool_.Schedule(callback);
  }

  // Schedules a new piece of work on the thread pool at the specified time
  // point.
  void ScheduleAt(const std::chrono::system_clock::time_point& when,
                  std::function<void()> callback);

 private:
  // An item of work which is scheduled for the future.
  struct ScheduledItem {
    ScheduledItem(const std::chrono::system_clock::time_point& when,
                  std::function<void()> callback) :
      when_(when), callback_(callback) {}

    std::chrono::system_clock::time_point when_;
    std::function<void()> callback_;
  };

  // This is the Executor's thread which is responsible for dealing with
  // ScheduleAt().
  void SchedulerThread();

  // A mutex to protect the shared state of the Executor.
  std::mutex mutex_;

  // Thread object for the scheduler thread. Note: This thread does not run
  // on the thread pool!
  std::unique_ptr<std::thread> scheduler_thread_;

  // The thread pool that this executor uses.
  ThreadPool thread_pool_;

  // A list of scheduled items. There is always one item in there, at
  // infinity. This makes inserting new items easier.
  std::list<ScheduledItem> scheduled_item_list_;

  // Instructs the scheduler thread to exit.
  bool please_exit_;

  // This condition variable indicates that the background thread needs to
  // be woken up because it needs to do some work.
  std::condition_variable wake_up_;
};

}

#endif  // YOUTUBE_DOORMAN_CXX_EXECUTOR_H_
