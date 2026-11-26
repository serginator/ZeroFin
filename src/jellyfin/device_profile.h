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
  // Direct play limits (Auto mode). Direct play decodes in software. On the CP0, 20 s of 720p
  // decode in 7.8 s, but 1080p needs 17 s (almost real time), so larger files use a server
  // transcode. Hardware decode (v4l2m2m) hangs the player on 720p and 1080p files.
  int direct_max_width = 1280;
  int direct_max_height = 720;
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
  // Transcode numbers for a monitor on the HDMI port (see profile_for). Direct play limits do not change.
  // Jellyfin gives 1080p only at 8 Mbps or more (it lowers the size below that), so keep the 1080p bitrate at 8 Mbps.
  int64_t hdmi720_video_bitrate = 3'000'000;
  int64_t hdmi1080_video_bitrate = 8'000'000;
  int64_t hdmi_audio_bitrate = 128'000;
  int hdmi_max_fps = 30;
};

// Where the video is shown. It picks the transcode size.
enum class Target { Lcd, Hdmi720, Hdmi1080 };

// The profile to use for a target. Lcd returns `base` as it is.
PlaybackProfile profile_for(const PlaybackProfile& base, Target target);

// Reads overrides from JSON. Missing keys keep the default. Unknown keys are ignored.
Result<PlaybackProfile> playback_profile_from_json(std::string_view json);

// Builds the "DeviceProfile" object sent in the PlaybackInfo request body.
// AlwaysTranscode leaves the direct play list empty.
nlohmann::json build_device_profile(const PlaybackProfile& profile, PlayMode mode);
// The same object as a JSON string.
std::string build_device_profile_json(const PlaybackProfile& profile, PlayMode mode);

}  // namespace zerofin::jf
