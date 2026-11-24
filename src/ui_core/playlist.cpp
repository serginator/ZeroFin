#include "ui_core/playlist.h"

#include <algorithm>

namespace zerofin::ui {

const jf::Item* adjacent_episode(const std::vector<jf::Item>& episodes, const std::string& current_id,
                                 int direction) {
  const auto it = std::find_if(episodes.begin(), episodes.end(), [&](const jf::Item& e) { return e.id == current_id; });
  if (it == episodes.end() || direction == 0) return nullptr;
  if (direction < 0) return it == episodes.begin() ? nullptr : &*(it - 1);
  const auto next = it + 1;
  return next == episodes.end() ? nullptr : &*next;
}

std::optional<int> track_ordinal(const std::vector<const jf::MediaStream*>& streams, int stream_index) {
  for (size_t i = 0; i < streams.size(); ++i) {
    if (streams[i] && streams[i]->index == stream_index) return static_cast<int>(i) + 1;
  }
  return std::nullopt;
}

}  // namespace zerofin::ui
