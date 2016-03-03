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
#include <chrono>
#include <iostream>

#include "cxx/interval_release_queue.h"

namespace doorman {

IntervalReleaseQueue::IntervalReleaseQueue(std::shared_ptr<Executor> executor)
    : executor_(executor),
      rate_limit_(0),
      interval_msec_(0),
      release_scheduled_(false) {}

IntervalReleaseQueue::IntervalReleaseQueue()
    : IntervalReleaseQueue(std::shared_ptr<Executor>(new Executor())) {}

// The destructor releases all waiting callbacks.
IntervalReleaseQueue::~IntervalReleaseQueue() {
  std::unique_lock<std::mutex> lock(mutex_);

  // If there is an invocation of _Release() scheduled that means that there
  // are callbacks waiting to be released. In that case we release them all
  // here.
  if (release_scheduled_) {
    // Sets this IntervalReleaseQueue's rate limit to infinite.
    rate_limit_ = -1;

    // We now wait until release_scheduled_ goes false, because that is a sign
    // that _Release has run. Because rate_limit_<0 a new release will not be
    // scheduled.
    destructor_.reset(new std::condition_variable);
    destructor_->wait(lock,
        [this]() -> bool { return !release_scheduled_; });
  }

  // At this point the wait_queue_ should be empty.
  assert(wait_queue_.empty());
}

// Returns the clock's value in msec.
int64_t IntervalReleaseQueue::_Now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             Clock::now().time_since_epoch())
      .count();
}

void IntervalReleaseQueue::SetRateLimit(int rate_limit, int interval_msec) {
  std::lock_guard<std::mutex> lock(mutex_);

  rate_limit_ = rate_limit;
  interval_msec_ = interval_msec;

  // If we increased the rate and there are callbacks waiting to be released
  // a call to Release() will release them.
  _Release();
}

// If there are elements in the time queue that happened more than an
// interval ago, we no longer care about them so we clear them.
void IntervalReleaseQueue::_ClearOldEvents(int64_t now) {
  const int64_t past_window = now - interval_msec_;

  while (!times_.empty() && *times_.begin() <= past_window) {
    times_.pop_front();
  }
}

// Releases whatever can be released right now and schedules a new invocation
// of itself if there is a need for that.
void IntervalReleaseQueue::_Release() {
  int64_t now = _Now();

  // Removes old events (that are before the current interval) from the times_
  // queue. This ensures that we have only events from the current interval
  // on the queue.
  _ClearOldEvents(now);

  // Releases callbacks as long as there is something to release and the current
  // interval is not yet exhausted,
  while ((rate_limit_ < 0 || times_.size() < rate_limit_) &&
         !wait_queue_.empty()) {
    times_.push_back(now);
    executor_->Schedule(wait_queue_.front());
    wait_queue_.pop_front();
  }

  // If there is no release scheduled yet and there are callbacks still in the
  // queue which need to be released starting at the next interval we schedule
  // a new invocation of Release().
  if (!release_scheduled_ && wait_queue_.size() > 0 && rate_limit_ != 0) {
    executor_->ScheduleIn(
        std::chrono::milliseconds(*times_.begin() + interval_msec_ - now),
        [this] {
          std::lock_guard<std::mutex> lock(mutex_);
          _Release();
          release_scheduled_ = false;

          if (destructor_.get() != nullptr) {
            destructor_->notify_one();
          }
        });
    release_scheduled_ = true;
  }
}

// Adds the callback to the waiting queue and calls Release() if
// there is a chance that it can be released immediately.
void IntervalReleaseQueue::Wait(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  wait_queue_.push_back(callback);

  // If there is currently a release scheduled that means that the current
  // interval is exhausted and there is no need to try and release this
  // callback.
  if (!release_scheduled_) {
    _Release();
  }
}

// The blocking version of Wait() is implemented in terms of the other Wait
// operation.
void IntervalReleaseQueue::Wait() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  std::condition_variable cond;
  bool signaled = false;

  Wait([&] {
      std::lock_guard<std::mutex> lock(mutex);
      signaled = true;
      cond.notify_one();
  });

  cond.wait(lock, [&]() -> bool { return signaled; });
}

}  // namespace doorman
