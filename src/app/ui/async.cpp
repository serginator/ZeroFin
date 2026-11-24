#include "ui/async.h"

namespace zerofin::app {

Async::Async() : worker_([this] { worker_loop(); }) {}

Async::~Async() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void Async::post(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
  }
  wake_.notify_one();
}

void Async::complete(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(mutex_);
  done_.push_back(std::move(task));
}

void Async::worker_loop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (stopping_) return;
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    task();
  }
}

void Async::pump() {
  std::deque<std::function<void()>> ready;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ready.swap(done_);
  }
  for (auto& task : ready) task();
}

}  // namespace zerofin::app
