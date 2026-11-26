#include "player/mpv_options.h"

namespace zerofin::player {
namespace {

// From spike-results.md. Scale the YUV frame first, so the RGB565 conversion is cheap.
constexpr const char* kScaleFilter =
    "scale=w=320:h=170:force_original_aspect_ratio=decrease:force_divisible_by=2:flags=fast_bilinear";

}  // namespace

std::string audio_device_name(AudioSink sink) {
  return sink == AudioSink::Hdmi ? "alsa/hdmi:CARD=vc4hdmi,DEV=0" : "auto";
}

MpvOptions mpv_options(const BackendOptions& options) {
  MpvOptions o;
  if (options.output == Output::Hdmi) {
    // Spike 2: vo=gpu with the DRM context played 720p with no dropped frames. vo=drm scales on the CPU.
    o.push_back({"vo", "gpu"});
    o.push_back({"gpu-context", "drm"});
    o.push_back({"drm-device", options.hdmi.device});
    o.push_back({"drm-connector", options.hdmi.connector});
    o.push_back({"audio-device", audio_device_name(options.audio)});
  } else {
    o.push_back({"vo", "libmpv"});
    o.push_back({"vf", kScaleFilter});
    o.push_back({"audio-device", "auto"});
  }
  o.push_back({"config", "no"});
  o.push_back({"terminal", "no"});
  o.push_back({"load-scripts", "no"});
  o.push_back({"ytdl", "no"});
  o.push_back({"input-default-bindings", "no"});
  o.push_back({"demuxer-max-bytes", "16MiB"});
  o.push_back({"demuxer-max-back-bytes", "1MiB"});
  o.push_back({"idle", "yes"});  // keep the core after the end of a file, so it never sends SHUTDOWN
  return o;
}

}  // namespace zerofin::player
