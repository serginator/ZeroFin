#include "ui_core/request_queue.h"

#include <algorithm>

namespace zerofin::ui {

void RequestQueue::set_wanted(std::vector<Entry> wanted) {
  waiting_.clear();
  for (auto& e : wanted) {
    if (in_flight_ && *in_flight_ == e.key) continue;
    const bool seen = std::any_of(waiting_.begin(), waiting_.end(), [&](const Entry& w) { return w.key == e.key; });
    if (!seen) waiting_.push_back(std::move(e));
  }
  std::stable_sort(waiting_.begin(), waiting_.end(),
                   [](const Entry& a, const Entry& b) { return a.priority < b.priority; });
}

std::optional<std::string> RequestQueue::next() {
  if (in_flight_ || waiting_.empty()) return std::nullopt;
  in_flight_ = std::move(waiting_.front().key);
  waiting_.erase(waiting_.begin());
  return in_flight_;
}

void RequestQueue::done(const std::string& key) {
  if (in_flight_ && *in_flight_ == key) in_flight_.reset();
}

}  // namespace zerofin::ui
