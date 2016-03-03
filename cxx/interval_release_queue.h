#ifndef YOUTUBE_DOORMAN_CXX_INTERVAL_RELEASE_QUEUE_H_
#define YOUTUBE_DOORMAN_CXX_INTERVAL_RELEASE_QUEUE_H_

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

#include "cxx/executor.h"

namespace doorman {

// This is an "internal use only" class for the Doorman rate limiter. The
// IntervalReleaseQueue releases its callers at a defined rate of "rate_limit"
// per "interval_msec". The internal timing uses the CPU cycle timer so it is
// highly accurate, even at small intervals. This IntervalReleaseQueue comes
// with two versions of Wait(), a blocking once which blocks the calling thread
// until it can be released and a non-blocking one which executes a callback.
class IntervalReleaseQueue {
 public:
  // Callbacks and internal scheduling are done on this Executor. A new
  // IntervalReleaseQueue is initially blocked (rate_limit is 0) until
  // you cann SetRateLimit.
  explicit IntervalReleaseQueue(std::shared_ptr<Executor> executor);

  // The default constructor initializes the queue with a default executor.
  IntervalReleaseQueue();

  // At destruction time all waiting callbacks and threads are released at
  // once.
  ~IntervalReleaseQueue();

  // Sets or resets the rate limit and interval.
  // Note: If the current interval is exhausted the new rate only goes into
  // effect at the start of the next interval.
  void SetRateLimit(int rate_limit, int interval_msec);

  // Adds a callback to the list of things to be released at the correct rate.
  // Note: Wait might not be the best name for this method.
  void Wait(std::function<void()> callback);

  // Waits until this caller can be released and then returns.
  void Wait();

 private:
  // Releases everything that can be released right now without violating the
  // rate limit. If necessary schedules a new invocation of itself on the
  // queue's Executor.
  void _Release();

  // Schedules a call to _Release() if one is necessary.
  void _ScheduleReleaseIfNecessary(int64_t now);

  // Removes old events from the times_ deque. Old events are events that
  // happened before the start of the current interval.
  void _ClearOldEvents(int64_t now);

  // Returns the system_clock's value in msec.
  int64_t _Now();

  // Guards the internal state of this object.
  mutable std::mutex mutex_;

  // Release and callbacks are executed by this Executor.
  std::shared_ptr<Executor> executor_;

  // The rate limit and interval of this IntervalReleaseQueue.
  int rate_limit_;
  int interval_msec_;

  // Indicates whether there is a future invocation of Release() scheduled.
  // When this is the case it also means that the current interval is
  // exhausted.
  bool release_scheduled_;

  // Every time a callback or thread gets released the timestamp of this
  // event (in msec) is added to this deque.
  std::deque<int64_t> times_;

  // This deque maintains all the callbacks which are waiting to be released.
  std::deque<std::function<void()>> wait_queue_;

  // Condition variable to be used to notify the destructor that the last
  // _Release has run.
  std::unique_ptr<std::condition_variable> destructor_;
};

}  // namespace doorman

#endif  // YOUTUBE_DOORMAN_CXX_INTERVAL_RELEASE_QUEUE_H_
