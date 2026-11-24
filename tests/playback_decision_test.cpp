#include <doctest/doctest.h>

#include "jellyfin/playback_decision.h"

using namespace zerofin::jf;

namespace {

struct Spec {
  const char* codec = "h264";
  const char* profile = "Main";
  int bit_depth = 8;
  int width = 1920;
  int height = 1080;
  int64_t source_bitrate = 5'200'000;
  int64_t video_bitrate = 0;
  bool interlaced = false;
  bool with_video = true;
};

MediaSource make_source(const Spec& s = {}) {
  MediaSource m;
  m.id = "ms1";
  m.bitrate = s.source_bitrate;
  if (s.with_video) {
    MediaStream v;
    v.index = 0;
    v.type = StreamType::Video;
    v.codec = s.codec;
    v.profile = s.profile;
    v.bit_depth = s.bit_depth;
    v.width = s.width;
    v.height = s.height;
    v.bitrate = s.video_bitrate;
    v.is_interlaced = s.interlaced;
    m.streams.push_back(v);
  }
  MediaStream audio;
  audio.index = 1;
  audio.type = StreamType::Audio;
  audio.codec = "aac";
  m.streams.push_back(audio);
  MediaStream text;
  text.index = 2;
  text.type = StreamType::Subtitle;
  text.codec = "subrip";
  text.is_text_subtitle = true;
  m.streams.push_back(text);
  MediaStream image;
  image.index = 3;
  image.type = StreamType::Subtitle;
  image.codec = "PGSSUB";
  m.streams.push_back(image);
  return m;
}

Decision decide(const Spec& s, PlayMode mode = PlayMode::Auto, std::optional<int> sub = std::nullopt) {
  return decide_playback(make_source(s), PlaybackProfile{}, mode, sub);
}

}  // namespace

TEST_CASE("H.264 Main 1080p under the bitrate cap plays directly") {
  const Decision d = decide(Spec{});
  CHECK(d.method == Method::DirectPlay);
  CHECK(d.reason == Reason::Ok);
  CHECK_FALSE(d.burn_in_subtitle);
}

TEST_CASE("a bitrate equal to the cap still plays directly") {
  Spec s;
  s.source_bitrate = 6'000'000;
  CHECK(decide(s).method == Method::DirectPlay);
}

TEST_CASE("a bitrate above the cap is transcoded") {
  Spec s;
  s.source_bitrate = 8'100'000;
  const Decision d = decide(s);
  CHECK(d.method == Method::Transcode);
  CHECK(d.reason == Reason::BitrateTooHigh);
}

TEST_CASE("the video bitrate is used when the source bitrate is missing") {
  Spec s;
  s.source_bitrate = 0;
  s.video_bitrate = 8'000'000;
  CHECK(decide(s).reason == Reason::BitrateTooHigh);
  s.video_bitrate = 2'000'000;
  CHECK(decide(s).method == Method::DirectPlay);
}

TEST_CASE("codecs other than H.264 are transcoded") {
  Spec s;
  s.codec = "hevc";
  s.profile = "Main";
  CHECK(decide(s).reason == Reason::VideoCodecUnsupported);
  s.codec = "mpeg4";
  CHECK(decide(s).reason == Reason::VideoCodecUnsupported);
  s.codec = "H264";  // case does not matter
  CHECK(decide(s).method == Method::DirectPlay);
}

TEST_CASE("10-bit H.264 is transcoded") {
  Spec s;
  s.profile = "High 10";
  s.bit_depth = 10;
  CHECK(decide(s).method == Method::Transcode);
  s.profile = "High";  // bit depth rule on its own
  CHECK(decide(s).reason == Reason::BitDepthTooHigh);
}

TEST_CASE("Baseline, Constrained Baseline, Main and High play directly; unknown profile too") {
  for (const char* profile : {"Baseline", "Constrained Baseline", "Main", "High", ""}) {
    Spec s;
    s.profile = profile;
    CHECK_MESSAGE(decide(s).method == Method::DirectPlay, profile);
  }
  Spec s;
  s.profile = "High 4:2:2";
  CHECK(decide(s).reason == Reason::UnsupportedH264Profile);
}

TEST_CASE("resolutions above 1080p are transcoded") {
  Spec s;
  s.width = 3840;
  s.height = 2160;
  CHECK(decide(s).reason == Reason::ResolutionTooHigh);
  s.width = 1920;
  s.height = 1200;
  CHECK(decide(s).reason == Reason::ResolutionTooHigh);
}

TEST_CASE("interlaced video is transcoded") {
  Spec s;
  s.interlaced = true;
  CHECK(decide(s).reason == Reason::Interlaced);
}

TEST_CASE("a source without video is transcoded") {
  Spec s;
  s.with_video = false;
  CHECK(decide(s).reason == Reason::NoVideo);
}

TEST_CASE("Always transcode forces the transcode path") {
  const Decision d = decide(Spec{}, PlayMode::AlwaysTranscode);
  CHECK(d.method == Method::Transcode);
  CHECK(d.reason == Reason::ForcedByUser);
}

TEST_CASE("a text subtitle keeps direct play") {
  const Decision d = decide(Spec{}, PlayMode::Auto, 2);
  CHECK(d.method == Method::DirectPlay);
  CHECK_FALSE(d.burn_in_subtitle);
}

TEST_CASE("an image subtitle forces transcode with burn-in") {
  const Decision d = decide(Spec{}, PlayMode::Auto, 3);
  CHECK(d.method == Method::Transcode);
  CHECK(d.reason == Reason::ImageSubtitleBurnIn);
  CHECK(d.burn_in_subtitle);
}

TEST_CASE("Always transcode with an image subtitle still burns it in") {
  const Decision d = decide(Spec{}, PlayMode::AlwaysTranscode, 3);
  CHECK(d.reason == Reason::ForcedByUser);
  CHECK(d.burn_in_subtitle);
}

TEST_CASE("an unknown subtitle index changes nothing") {
  CHECK(decide(Spec{}, PlayMode::Auto, 42).method == Method::DirectPlay);
}

TEST_CASE("is_image_subtitle") {
  auto sub = [](const char* codec, bool text) {
    MediaStream s;
    s.type = StreamType::Subtitle;
    s.codec = codec;
    s.is_text_subtitle = text;
    return s;
  };
  CHECK(is_image_subtitle(sub("PGSSUB", false)));
  CHECK(is_image_subtitle(sub("hdmv_pgs_subtitle", false)));
  CHECK(is_image_subtitle(sub("dvd_subtitle", false)));
  CHECK(is_image_subtitle(sub("dvbsub", false)));
  CHECK_FALSE(is_image_subtitle(sub("subrip", true)));
  CHECK_FALSE(is_image_subtitle(sub("srt", false)));
  CHECK_FALSE(is_image_subtitle(sub("ass", true)));
  CHECK_FALSE(is_image_subtitle(sub("mov_text", false)));
  CHECK(is_image_subtitle(sub("somethingnew", false)));  // unknown and not text: assume image

  MediaStream audio;
  audio.type = StreamType::Audio;
  CHECK_FALSE(is_image_subtitle(audio));
}
