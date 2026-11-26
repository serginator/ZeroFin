#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "player/mpv_options.h"

using namespace zerofin::player;

namespace {

std::string value_of(const MpvOptions& options, const std::string& key) {
  for (const auto& kv : options) {
    if (kv.first == key) return kv.second;
  }
  return "<none>";
}

BackendOptions hdmi() {
  BackendOptions o;
  o.output = Output::Hdmi;
  o.hdmi = HdmiConnector{"/dev/dri/card0", "HDMI-A-1"};
  return o;
}

}  // namespace

TEST_CASE("mpv options: the LCD draws into the app and scales the picture down") {
  const MpvOptions o = mpv_options(BackendOptions{});
  CHECK(value_of(o, "vo") == "libmpv");
  CHECK(value_of(o, "vf").find("scale=w=320:h=170") == 0);
  CHECK(value_of(o, "gpu-context") == "<none>");
}

TEST_CASE("mpv options: a monitor gets its own GPU output on the HDMI connector") {
  const MpvOptions o = mpv_options(hdmi());
  CHECK(value_of(o, "vo") == "gpu");
  CHECK(value_of(o, "gpu-context") == "drm");
  CHECK(value_of(o, "drm-device") == "/dev/dri/card0");
  CHECK(value_of(o, "drm-connector") == "HDMI-A-1");
  CHECK(value_of(o, "vf") == "<none>");  // no 320x170 scaling on the monitor
}

TEST_CASE("mpv options: both outputs share the safe defaults") {
  for (const BackendOptions& b : {BackendOptions{}, hdmi()}) {
    const MpvOptions o = mpv_options(b);
    CHECK(value_of(o, "config") == "no");
    CHECK(value_of(o, "terminal") == "no");
    CHECK(value_of(o, "ytdl") == "no");
    CHECK(value_of(o, "idle") == "yes");
    CHECK(value_of(o, "demuxer-max-bytes") == "16MiB");
  }
}

TEST_CASE("mpv options: the sound goes to the monitor only when asked") {
  CHECK(audio_device_name(AudioSink::Default) == "auto");
  CHECK(audio_device_name(AudioSink::Hdmi) == "alsa/hdmi:CARD=vc4hdmi,DEV=0");

  CHECK(value_of(mpv_options(hdmi()), "audio-device") == "auto");
  BackendOptions o = hdmi();
  o.audio = AudioSink::Hdmi;
  CHECK(value_of(mpv_options(o), "audio-device") == "alsa/hdmi:CARD=vc4hdmi,DEV=0");
}

TEST_CASE("mpv options: the LCD never sends sound to HDMI by itself") {
  BackendOptions o;  // Lcd
  o.audio = AudioSink::Hdmi;
  CHECK(value_of(mpv_options(o), "audio-device") == "auto");
}
