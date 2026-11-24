#include "jellyfin/playback_decision.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "jellyfin/text.h"

namespace zerofin::jf {
namespace {

// H.264 profiles the hardware decoder is expected to handle in 8-bit.
// An empty profile (unknown) is accepted. High 10 and 4:2:2 are caught by the bit depth rule
// or are listed as unsupported here.
bool h264_profile_supported(const std::string& profile) {
  const std::string p = to_lower(profile);
  if (p.empty()) return true;
  return p == "baseline" || p == "constrained baseline" || p == "main" || p == "high";
}

}  // namespace

bool is_image_subtitle(const MediaStream& s) {
  if (s.type != StreamType::Subtitle) return false;
  const std::string codec = to_lower(s.codec);
  if (codec == "pgssub" || codec == "pgs" || codec == "dvdsub" || codec == "dvd_subtitle" || codec == "dvbsub" ||
      codec == "dvb_subtitle" || codec == "hdmv_pgs_subtitle" || codec == "xsub") {
    return true;
  }
  return !s.is_text_subtitle && codec != "srt" && codec != "subrip" && codec != "ass" && codec != "ssa" &&
         codec != "webvtt" && codec != "vtt" && codec != "mov_text";
}

Decision decide_playback(const MediaSource& source, const PlaybackProfile& profile, PlayMode mode,
                         std::optional<int> subtitle_index) {
  const MediaStream* video = source.video();
  if (!video) return {Method::Transcode, Reason::NoVideo, false};

  bool burn_in = false;
  if (subtitle_index) {
    if (const MediaStream* sub = source.stream_by_index(*subtitle_index)) burn_in = is_image_subtitle(*sub);
  }

  if (mode == PlayMode::AlwaysTranscode) return {Method::Transcode, Reason::ForcedByUser, burn_in};
  if (burn_in) return {Method::Transcode, Reason::ImageSubtitleBurnIn, true};

  if (to_lower(video->codec) != "h264") return {Method::Transcode, Reason::VideoCodecUnsupported, false};
  if (!h264_profile_supported(video->profile)) return {Method::Transcode, Reason::UnsupportedH264Profile, false};
  if (video->bit_depth > profile.direct_max_bit_depth) return {Method::Transcode, Reason::BitDepthTooHigh, false};
  if (video->width > profile.direct_max_width || video->height > profile.direct_max_height) {
    return {Method::Transcode, Reason::ResolutionTooHigh, false};
  }
  const int64_t bitrate = source.bitrate > 0 ? source.bitrate : video->bitrate;
  if (bitrate > profile.direct_max_bitrate) return {Method::Transcode, Reason::BitrateTooHigh, false};
  if (video->is_interlaced) return {Method::Transcode, Reason::Interlaced, false};

  return {Method::DirectPlay, Reason::Ok, false};
}

}  // namespace zerofin::jf
