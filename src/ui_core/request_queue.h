#pragma once

#include <optional>
#include <string>
#include <vector>

namespace zerofin::ui {

// The posters a screen wants, in priority order, and the one that is downloading now.
// One request runs at a time. UI thread only.
class RequestQueue {
 public:
  struct Entry {
    std::string key;
    int priority = 0;  // a lower number goes first
  };

  // Replaces the waiting entries. The key in flight is not queued again. Equal priorities keep the caller's order.
  void set_wanted(std::vector<Entry> wanted);
  // The next key to download, or nullopt while one is in flight or nothing waits. Marks it in flight.
  std::optional<std::string> next();
  void done(const std::string& key);
  bool in_flight() const { return in_flight_.has_value(); }

 private:
  std::vector<Entry> waiting_;
  std::optional<std::string> in_flight_;
};

}  // namespace zerofin::ui
