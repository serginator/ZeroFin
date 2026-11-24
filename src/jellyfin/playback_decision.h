#pragma once

#include <optional>

#include "jellyfin/device_profile.h"
#include "jellyfin/models.h"

namespace zerofin::jf {

enum class Method { DirectPlay, Transcode };

enum class Reason {
  Ok,                    // direct play is allowed
  ForcedByUser,          // "Always transcode" is on
  NoVideo,
  VideoCodecUnsupported, // only H.264 decodes in hardware
  UnsupportedH264Profile,
  BitDepthTooHigh,       // hardware decode handles 8-bit only
  ResolutionTooHigh,
  BitrateTooHigh,
  Interlaced,
  ImageSubtitleBurnIn    // image subtitles need server burn-in
};

struct Decision {
  Method method = Method::Transcode;
  Reason reason = Reason::NoVideo;
  bool burn_in_subtitle = false;  // true when the server must draw the chosen subtitle
};

bool is_image_subtitle(const MediaStream& stream);

// Decides how to play. `subtitle_index` is the chosen subtitle stream, or nullopt for none.
// Bitrate rule: uses the source bitrate, or the video bitrate when the source has none.
Decision decide_playback(const MediaSource& source, const PlaybackProfile& profile, PlayMode mode,
                         std::optional<int> subtitle_index);

}  // namespace zerofin::jf
