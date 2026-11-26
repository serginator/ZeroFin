#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "jellyfin/device_profile.h"

using namespace zerofin::jf;
using nlohmann::json;

namespace {

const json* find_condition(const json& conditions, const std::string& property) {
  for (const auto& c : conditions) {
    if (c["Property"] == property) return &c;
  }
  return nullptr;
}

}  // namespace

TEST_CASE("Auto profile allows H.264 direct play within the caps") {
  const json p = json::parse(build_device_profile_json(PlaybackProfile{}, PlayMode::Auto));
  REQUIRE(p["DirectPlayProfiles"].size() == 1);
  CHECK(p["DirectPlayProfiles"][0]["VideoCodec"] == "h264");

  const json& cond = p["CodecProfiles"][0]["Conditions"];
  CHECK(p["CodecProfiles"][0]["Codec"] == "h264");
  CHECK((*find_condition(cond, "Width"))["Value"] == "1280");
  CHECK((*find_condition(cond, "Height"))["Value"] == "720");
  CHECK((*find_condition(cond, "VideoBitDepth"))["Value"] == "8");
  CHECK((*find_condition(cond, "VideoBitrate"))["Value"] == "6000000");
  CHECK((*find_condition(cond, "VideoBitrate"))["Condition"] == "LessThanEqual");
}

TEST_CASE("AlwaysTranscode profile has no direct play entries") {
  const json p = json::parse(build_device_profile_json(PlaybackProfile{}, PlayMode::AlwaysTranscode));
  CHECK(p["DirectPlayProfiles"].empty());
  CHECK(p["TranscodingProfiles"].size() == 1);
}

TEST_CASE("transcoding profile asks for H.264 and AAC over HLS in 3 s segments") {
  const json p = json::parse(build_device_profile_json(PlaybackProfile{}, PlayMode::Auto));
  const json& t = p["TranscodingProfiles"][0];
  CHECK(t["Protocol"] == "hls");
  CHECK(t["Container"] == "ts");
  CHECK(t["VideoCodec"] == "h264");
  CHECK(t["AudioCodec"] == "aac");
  CHECK(t["MaxAudioChannels"] == "2");
  CHECK(t["SegmentLength"] == 3);
  CHECK(p["MaxStreamingBitrate"] == 1'296'000);
}

TEST_CASE("text subtitles are external and image subtitles are burned in") {
  const json p = json::parse(build_device_profile_json(PlaybackProfile{}, PlayMode::Auto));
  std::string srt, pgs;
  for (const auto& s : p["SubtitleProfiles"]) {
    if (s["Format"] == "srt") srt = s["Method"];
    if (s["Format"] == "pgssub") pgs = s["Method"];
  }
  CHECK(srt == "External");
  CHECK(pgs == "Encode");
}

TEST_CASE("profile numbers follow the PlaybackProfile values") {
  PlaybackProfile profile;
  profile.direct_max_bitrate = 4'000'000;
  profile.transcode_audio_channels = 6;
  const json p = json::parse(build_device_profile_json(profile, PlayMode::Auto));
  CHECK((*find_condition(p["CodecProfiles"][0]["Conditions"], "VideoBitrate"))["Value"] == "4000000");
  CHECK(p["TranscodingProfiles"][0]["MaxAudioChannels"] == "6");
}

TEST_CASE("playback_profile_from_json overrides listed keys only") {
  auto r = playback_profile_from_json(R"({"direct_max_bitrate":4000000,"transcode_max_fps":15,"unknown":1})");
  REQUIRE(r.ok());
  CHECK(r.value().direct_max_bitrate == 4'000'000);
  CHECK(r.value().transcode_max_fps == 15);
  CHECK(r.value().direct_max_width == 1280);  // default kept
  CHECK(r.value().transcode_max_width == 320);
}

TEST_CASE("playback_profile_from_json rejects bad input") {
  CHECK(playback_profile_from_json("[]").error().kind == ErrorKind::Parse);
  CHECK(playback_profile_from_json("{").error().kind == ErrorKind::Parse);
}
