#include <cassert>
#include <iostream>
#include <limits>

#include "cxx/thread_pool.h"

namespace doorman {

void ThreadPool::WorkQueue::Put(std::function<void()> item) {
  if (!shutdown_) {
    std::lock_guard<std::mutex> lock(mutex_);

    q_.push_back(item);
    work_.notify_one();
  }
}

std::function<void()> ThreadPool::WorkQueue::Get() {
  std::unique_lock<std::mutex> lock(mutex_);

  while (q_.size() == 0) {
    work_.wait(lock);

    if (shutdown_) {
      lock.unlock();
      return Dummy;
    }
  }

  std::function<void()> item = q_.front();

  q_.pop_front();
  lock.unlock();

  return item;
}

ThreadPool::ThreadPool() : ThreadPool(0, std::numeric_limits<int>::max()) {}

ThreadPool::ThreadPool(int min, int max)
    : min_(min),
      max_(max),
      num_theads_created_(0),
      num_threads_(0),
      busy_threads_(0),
      shutdown_(false) {
  assert(min_ >= 0);
  assert(max_ >= min_);
  assert(max_ > 0);

  for (int i = 0; i < min_; ++i) {
    CreateThread();
  }
}

ThreadPool::~ThreadPool() {
  shutdown_ = true;
  work_q_.Shutdown();

  std::lock_guard<std::mutex> lock(mutex_);

  for (auto iter = threads_.begin(); iter != threads_.end(); ++iter) {
    iter->second.join();
  }
}

void ThreadPool::Schedule(std::function<void()> callback) {
  if (busy_threads_ == num_threads_ && num_threads_ < max_) {
    ++stats_.num_grow_events;
    CreateThread();
  }

  work_q_.Put(callback);
}

// Warning: CreateThread should only be called in a context where you hold the
// mutex or are sure there is no contention (e.g. from the constructor).
void ThreadPool::CreateThread() {
  std::lock_guard<std::mutex> lock(mutex_);

  // Ensures that we do not grow beyond the maximum allowed.
  if (num_threads_ < max_) {
    ++num_threads_;
    ++num_theads_created_;
    threads_[num_theads_created_] =
        std::thread(&ThreadPool::RunThread, this, num_theads_created_);
  }
}

void ThreadPool::RunThread(int index) {
  while (true) {
    // Gets a work item from the queue and executes it.
    std::function<void()> item = work_q_.Get();

    if (shutdown_) {
      return;
    }

    ++busy_threads_;
    item();
    --busy_threads_;

    // Figures out if it is time for this thread to end.
    std::lock_guard<std::mutex> lock(mutex_);

    if (num_threads_ > min_ && work_q_.Size() == 0) {
      ++stats_.num_shrink_events;
      --num_threads_;
      threads_[index].detach();
      threads_.erase(index);
      return;
    }
  }
}

ThreadPool::Stats ThreadPool::GetStats() {
  Stats result;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    result = stats_;
  }

  result.current_size = num_threads_;
  result.num_busy_threads = busy_threads_;

  return result;
}

}  // namespace doorman
