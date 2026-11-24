#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

#include "jellyfin/result.h"

namespace zerofin::jf {

enum class PlayMode { Auto, AlwaysTranscode };

// The one place that holds the playback numbers. Loadable from a JSON file.
struct PlaybackProfile {
  // Direct play limits (Auto mode).
  int direct_max_width = 1920;
  int direct_max_height = 1080;
  int direct_max_bit_depth = 8;
  int64_t direct_max_bitrate = 6'000'000;  // total source bitrate, bits per second
  // Server transcode target. The screen is 320x170.
  int transcode_max_width = 320;
  int transcode_max_height = 170;
  int transcode_max_fps = 24;
  int64_t transcode_video_bitrate = 1'200'000;
  int64_t transcode_audio_bitrate = 96'000;
  int transcode_audio_channels = 2;
  int transcode_segment_seconds = 3;
};

// Reads overrides from JSON. Missing keys keep the default. Unknown keys are ignored.
Result<PlaybackProfile> playback_profile_from_json(std::string_view json);

// Builds the "DeviceProfile" object sent in the PlaybackInfo request body.
// AlwaysTranscode leaves the direct play list empty.
nlohmann::json build_device_profile(const PlaybackProfile& profile, PlayMode mode);
// The same object as a JSON string.
std::string build_device_profile_json(const PlaybackProfile& profile, PlayMode mode);

}  // namespace zerofin::jf
