#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

namespace zerofin::app {

// Runs blocking work (network calls) on one worker thread and delivers the result on the
// UI thread. LVGL is used from the UI thread only. One worker means requests run in order,
// so the Jellyfin client needs no locking.
class Async {
 public:
  // A screen holds an Alive token and drops it when it goes away. Results for a dead
  // token are discarded.
  using Alive = std::shared_ptr<bool>;
  static Alive make_alive() { return std::make_shared<bool>(true); }

  Async();
  ~Async();
  Async(const Async&) = delete;
  Async& operator=(const Async&) = delete;

  // `work` runs on the worker. `done` runs on the UI thread, inside pump(), if `alive` is still true.
  template <typename Work, typename Done>
  void run(Alive alive, Work work, Done done) {
    using R = std::invoke_result_t<Work>;
    ++pending_;
    post([this, alive = std::move(alive), work = std::move(work), done = std::move(done)]() mutable {
      auto result = std::make_shared<R>(work());
      complete([this, alive = std::move(alive), done = std::move(done), result]() mutable {
        --pending_;
        if (*alive) done(std::move(*result));
      });
    });
  }

  // Call on the UI thread, often. Runs the finished callbacks.
  void pump();

  // True while work is queued, running, or waiting for pump().
  bool busy() const { return pending_ > 0; }

 private:
  void post(std::function<void()> task);
  void complete(std::function<void()> task);
  void worker_loop();

  std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<std::function<void()>> tasks_;
  std::deque<std::function<void()>> done_;
  bool stopping_ = false;
  int pending_ = 0;  // touched on the UI thread only
  std::thread worker_;
};

}  // namespace zerofin::app
