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

TEST_CASE("profile_for(Lcd) is the base profile, unchanged") {
  PlaybackProfile base;
  base.transcode_video_bitrate = 900'000;
  const PlaybackProfile p = profile_for(base, Target::Lcd);
  CHECK(p.transcode_max_width == 320);
  CHECK(p.transcode_max_height == 170);
  CHECK(p.transcode_max_fps == 24);
  CHECK(p.transcode_video_bitrate == 900'000);
  CHECK(p.transcode_audio_bitrate == 96'000);
}

TEST_CASE("profile_for(Hdmi720) transcodes to 1280x720 at 3 Mbps") {
  const PlaybackProfile p = profile_for(PlaybackProfile{}, Target::Hdmi720);
  CHECK(p.transcode_max_width == 1280);
  CHECK(p.transcode_max_height == 720);
  CHECK(p.transcode_max_fps == 30);
  CHECK(p.transcode_video_bitrate == 3'000'000);
  CHECK(p.transcode_audio_bitrate == 128'000);
}

TEST_CASE("profile_for(Hdmi1080) transcodes to 1920x1080 at 8 Mbps") {
  // Jellyfin returns 720p when a 1080p request has less than 8 Mbps.
  const PlaybackProfile p = profile_for(PlaybackProfile{}, Target::Hdmi1080);
  CHECK(p.transcode_max_width == 1920);
  CHECK(p.transcode_max_height == 1080);
  CHECK(p.transcode_video_bitrate == 8'000'000);
  CHECK(p.transcode_audio_bitrate == 128'000);
}

TEST_CASE("HDMI targets keep the direct play limits (a 1080p file is always transcoded)") {
  for (Target t : {Target::Lcd, Target::Hdmi720, Target::Hdmi1080}) {
    const PlaybackProfile p = profile_for(PlaybackProfile{}, t);
    CHECK(p.direct_max_width == 1280);
    CHECK(p.direct_max_height == 720);
    CHECK(p.direct_max_bitrate == 6'000'000);
  }
}

TEST_CASE("profile.json can change the HDMI numbers") {
  auto r = playback_profile_from_json(R"({"hdmi720_video_bitrate":4000000,"hdmi1080_video_bitrate":9000000,)"
                                      R"("hdmi_max_fps":24,"hdmi_audio_bitrate":192000})");
  REQUIRE(r.ok());
  const PlaybackProfile a = profile_for(r.value(), Target::Hdmi720);
  CHECK(a.transcode_video_bitrate == 4'000'000);
  CHECK(a.transcode_max_fps == 24);
  CHECK(a.transcode_audio_bitrate == 192'000);
  CHECK(profile_for(r.value(), Target::Hdmi1080).transcode_video_bitrate == 9'000'000);
}

TEST_CASE("the device profile for HDMI 1080p allows 8.1 Mbps of streaming") {
  const PlaybackProfile p = profile_for(PlaybackProfile{}, Target::Hdmi1080);
  CHECK(build_device_profile(p, PlayMode::AlwaysTranscode)["MaxStreamingBitrate"] == 8'128'000);
}
