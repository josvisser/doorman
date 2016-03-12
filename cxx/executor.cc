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

#include <cassert>
#include <climits>
#include <iostream>

#include "cxx/executor.h"

namespace doorman {

Executor::Executor(int min, int max) : thread_pool_(min, max), please_exit_(false) {
  // Inserts a dummy callback at the end of time in the list of scheduled work
  // items.
  scheduled_item_list_.push_back(
      ScheduledItem(Clock::time_point::max(),
                    [] { throw "This should never happen"; }));

  // Starts the scheduler thread.
  scheduler_thread_.reset(new std::thread(&Executor::_SchedulerThread, this));
}

Executor::~Executor() {
  {
    // Wakes up the scheduler thread and tells it to exit.
    std::lock_guard<std::mutex> lock(mutex_);

    please_exit_ = true;
    wake_up_.notify_one();
  }

  // Waits for the scheduler thread to terminate.
  scheduler_thread_->join();
}

void Executor::ScheduleAt(const Clock::time_point& when,
                          std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto first_event_time = scheduled_item_list_.front().when_;

  // Inserts the new scheduler item in the list of scheduled items.
  for (auto iter = scheduled_item_list_.begin(); ; ++iter) {
    if (iter->when_ > when) {
      scheduled_item_list_.insert(iter, ScheduledItem(when, callback));
      break;
    }
  }

  // Checks to see if we need to wake up the scheduler thread. This is the case
  // when we inserted an event at the start of the scheduled item list.
  if (first_event_time != scheduled_item_list_.front().when_) {
    wake_up_.notify_one();
  }
}

void Executor::_SchedulerThread() {
  std::unique_lock<std::mutex> lock(mutex_);

  for (;;) {
    // Waits until either the first item in the scheduled item list needs
    // execution or we got woken up to exit or because there is a new item at
    // the front of the list.
    wake_up_.wait_until(lock, scheduled_item_list_.front().when_);

    // Did the destructor ask us to please exit?
    if (please_exit_) {
      lock.unlock();
      return;
    }

    // Pops items of the front of the list and schedules them on the thread pool
    // until we caught up.
    auto now = std::chrono::system_clock::now();

    while (scheduled_item_list_.front().when_ <= now) {
      thread_pool_.Schedule(scheduled_item_list_.front().callback_);
      scheduled_item_list_.pop_front();
    }
  }
}

}  // namespace doorman
