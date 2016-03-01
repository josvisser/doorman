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

#ifndef YOUTUBE_DOORMAN_CXX_THREAD_POOL_H_
#define YOUTUBE_DOORMAN_CXX_THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace doorman {

// ThreadPool is, well, a thread pool :-)
class ThreadPool {
 public:
  // Thread pool stats are returned to the caller in this structure.
  struct Stats {
    Stats()
        : current_size(0),
          num_busy_threads(0),
          num_shrink_events(0),
          num_grow_events(0) {}

    int current_size;
    int num_busy_threads;
    int num_shrink_events;
    int num_grow_events;
  };

  // The default constructor creates a threadpool with an unbounded number of
  // threads in it.
  ThreadPool();
  ThreadPool(int min, int max);

  // The destructor waits until all threads have ended.
  ~ThreadPool();

  // Schedules a callback on a thread from the thread pool. If the thread pool
  // is maxed out this method will put the callback on a queue until a thread
  // becomes available.
  void Schedule(std::function<void()> callback);

  // Returns a copy of the thread pool stats.
  Stats GetStats();

 private:
  // This is a thread safe queue for putting work items on.
  class WorkQueue {
   public:
    WorkQueue() : shutdown_(false){};

    // Puts a new work item on the queue.
    void Put(std::function<void()> item);

    // Gets a work item from the queue. Blocks until a work item is available.
    std::function<void()> Get();

    // Shuts down the queue. From that moment on all calls
    // to put() will silently return and all calls to get() will return a
    // dummy item.
    void Shutdown() {
      shutdown_ = true;
      work_.notify_all();
    }

    // This is a dummy work item method which will be returned when the queue
    // is shutting down.
    static void Dummy() {}

    // Returns the size of the queue.
    int Size() {
      std::lock_guard<std::mutex> lock(mutex_);
      return q_.size();
    }

   private:
    // This mutex is protecting the queue.
    std::mutex mutex_;

    // The actual queue of work.
    std::deque<std::function<void()>> q_;

    // Condition variable for putting work on the queue.
    std::condition_variable work_;

    // Indicator that the queue is shutting down.
    std::atomic_bool shutdown_;
  };

  // Creates a new worker thread in the pool.
  void _CreateThread();

  // Each thread in the thread pool runs this main function. Passes in the
  // internal index in the threads_ vector.
  void _RunThread(int index);

  // This mutex protects the shared state of the thread pool.
  std::mutex mutex_;

  // This map keeps the thread objects for the currently running threads.
  std::map<int, std::thread> threads_;

  // Queue for the work items that this thread pool is working on.
  WorkQueue work_q_;

  // Minimum number of threads in the thread pool.
  int min_;

  // Maximum number of threads in the thread pool.
  int max_;

  // Keeps track of how many threads this thread pool has created.
  int num_theads_created_;

  // Current number of threads in the thread pool.
  std::atomic_int num_threads_;

  // Counts the number of busy threads.
  std::atomic_int busy_threads_;

  // This flag indicates that the thread pool is shutting down. It instructs
  // all running threads to exit, regardless of the amount of work in the
  // queue.
  std::atomic_bool shutdown_;

  // Threadpool stats.
  Stats stats_;
};
}

#endif  // YOUTUBE_DOORMAN_CXX_THREAD_POOL_H_
