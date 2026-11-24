#pragma once

#include <cstdint>
#include <string>

#include "jellyfin/models.h"

namespace zerofin::ui {

// "45m", "1h 52m", "2h". Empty when the runtime is unknown.
std::string format_runtime(int64_t ticks);

// "0:07", "12:34", "1:02:03". Negative values count as 0.
std::string format_timecode(double seconds);

// "S2E5", "E5" (no season), or empty.
std::string episode_label(const jf::Item& item);

// "5.2 Mbps", "850 kbps", empty when unknown.
std::string format_bitrate(int64_t bits_per_second);

// "English", "Japanese", or the upper-case code for unknown ones. Empty or "und" -> "Unknown".
std::string language_name(const std::string& iso_code);

// "English · AAC 2.0", "Japanese · EAC3 5.1".
std::string audio_label(const jf::MediaStream& stream);

// "English · SRT", "English · PGS (burned in)", with "forced" when flagged.
std::string subtitle_label(const jf::MediaStream& stream);

// Cuts the text to `max_chars` characters (not bytes) and adds "..." when it was longer.
std::string truncate_text(const std::string& text, size_t max_chars);

enum class RowContext {
  Browse,    // a library, season or search list
  Continue,  // Continue Watching and Next Up on the home screen
};

struct RowText {
  std::string primary;
  std::string detail;
  double progress = -1;  // 0..1 for a progress bar, negative for none
  bool watched = false;
};

// The text of one item row.
RowText describe_item(const jf::Item& item, RowContext context);

}  // namespace zerofin::ui
