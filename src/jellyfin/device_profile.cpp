#include "jellyfin/device_profile.h"

#include <nlohmann/json.hpp>

namespace zerofin::jf {
namespace {

using nlohmann::json;

template <typename T>
void read(const json& j, const char* key, T& out) {
  auto it = j.find(key);
  if (it != j.end() && it->is_number()) out = it->get<T>();
}

json condition(const char* cond, const char* property, const std::string& value, bool required = true) {
  return {{"Condition", cond}, {"Property", property}, {"Value", value}, {"IsRequired", required}};
}

}  // namespace

Result<PlaybackProfile> playback_profile_from_json(std::string_view text) {
  try {
    json j = json::parse(text);
    if (!j.is_object()) return Error{ErrorKind::Parse, "profile must be a JSON object", 0};
    PlaybackProfile p;
    read(j, "direct_max_width", p.direct_max_width);
    read(j, "direct_max_height", p.direct_max_height);
    read(j, "direct_max_bit_depth", p.direct_max_bit_depth);
    read(j, "direct_max_bitrate", p.direct_max_bitrate);
    read(j, "transcode_max_width", p.transcode_max_width);
    read(j, "transcode_max_height", p.transcode_max_height);
    read(j, "transcode_max_fps", p.transcode_max_fps);
    read(j, "transcode_video_bitrate", p.transcode_video_bitrate);
    read(j, "transcode_audio_bitrate", p.transcode_audio_bitrate);
    read(j, "transcode_audio_channels", p.transcode_audio_channels);
    read(j, "transcode_segment_seconds", p.transcode_segment_seconds);
    return p;
  } catch (const json::exception& e) {
    return Error{ErrorKind::Parse, e.what(), 0};
  }
}

std::string build_device_profile_json(const PlaybackProfile& p, PlayMode mode) {
  json profile;
  profile["Name"] = "ZeroFin";
  profile["MaxStreamingBitrate"] = p.transcode_video_bitrate + p.transcode_audio_bitrate;
  profile["MaxStaticBitrate"] = p.direct_max_bitrate;

  json direct = json::array();
  if (mode == PlayMode::Auto) {
    direct.push_back({{"Type", "Video"},
                      {"Container", "mp4,m4v,mkv,mov,avi,ts,webm"},
                      {"VideoCodec", "h264"},
                      {"AudioCodec", "aac,ac3,eac3,mp3,opus,flac,vorbis"}});
  }
  profile["DirectPlayProfiles"] = direct;

  profile["TranscodingProfiles"] = json::array({{{"Type", "Video"},
                                                 {"Container", "ts"},
                                                 {"Protocol", "hls"},
                                                 {"Context", "Streaming"},
                                                 {"VideoCodec", "h264"},
                                                 {"AudioCodec", "aac"},
                                                 {"MaxAudioChannels", std::to_string(p.transcode_audio_channels)},
                                                 {"SegmentLength", p.transcode_segment_seconds},
                                                 {"MinSegments", 1},
                                                 {"BreakOnNonKeyFrames", true}}});

  json h264_conditions = json::array();
  h264_conditions.push_back(condition("LessThanEqual", "VideoBitDepth", std::to_string(p.direct_max_bit_depth)));
  h264_conditions.push_back(condition("LessThanEqual", "Width", std::to_string(p.direct_max_width)));
  h264_conditions.push_back(condition("LessThanEqual", "Height", std::to_string(p.direct_max_height)));
  h264_conditions.push_back(condition("LessThanEqual", "VideoBitrate", std::to_string(p.direct_max_bitrate)));
  profile["CodecProfiles"] = json::array({{{"Type", "Video"}, {"Codec", "h264"}, {"Conditions", h264_conditions}}});

  // Text subtitles come as files that libmpv draws. Image subtitles must be burned in.
  json subs = json::array();
  for (const char* fmt : {"srt", "subrip", "ass", "ssa", "vtt", "webvtt"}) {
    subs.push_back({{"Format", fmt}, {"Method", "External"}});
  }
  for (const char* fmt : {"pgs", "pgssub", "dvdsub", "dvbsub"}) {
    subs.push_back({{"Format", fmt}, {"Method", "Encode"}});
  }
  profile["SubtitleProfiles"] = subs;

  return profile.dump();
}

}  // namespace zerofin::jf
