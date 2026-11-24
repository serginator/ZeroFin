#pragma once

#include <optional>
#include <string>
#include <vector>

#include "jellyfin/models.h"

namespace zerofin::ui {

// The episode before (`direction` < 0) or after (`direction` > 0) `current_id` in list order.
// Null at either end, for an empty list, and when `current_id` is not in the list.
const jf::Item* adjacent_episode(const std::vector<jf::Item>& episodes, const std::string& current_id,
                                 int direction);

// The 1-based place of the stream with Jellyfin index `stream_index` among `streams` (the audio
// streams, or the subtitle streams, in file order). That is the track number libmpv uses.
// Null when the stream is not in the list.
std::optional<int> track_ordinal(const std::vector<const jf::MediaStream*>& streams, int stream_index);

}  // namespace zerofin::ui
