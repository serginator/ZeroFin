#pragma once

#include <utility>

#include "player/backend.h"

struct mpv_handle;
struct mpv_render_context;

namespace zerofin::player {

// Plays with libmpv. On the LCD it uses the software render API and a scale filter, so the RGB565
// conversion only touches a small picture (spike-results.md). With a monitor it lets mpv open its own
// GPU output on the HDMI connector (hdmi-output spike 2), and the app draws nothing for the video.
class MpvBackend : public PlayerBackend {
 public:
  explicit MpvBackend(BackendOptions options = {}) : options_(std::move(options)) {}
  ~MpvBackend() override;
  MpvBackend(const MpvBackend&) = delete;
  MpvBackend& operator=(const MpvBackend&) = delete;

  bool start(const StartParams& params) override;
  PlayerState poll(uint32_t now_ms) override;
  bool render(uint8_t* rgb565, size_t stride) override;
  void set_paused(bool paused) override;
  void seek_relative(double seconds) override;
  void set_volume(int percent) override;
  void set_audio_sink(AudioSink sink) override;
  void stop() override;

 private:
  bool create_handle();
  void drain_events();
  void refresh_properties(uint32_t now_ms);
  void fail(const std::string& what);

  const BackendOptions options_;
  mpv_handle* mpv_ = nullptr;
  mpv_render_context* render_ = nullptr;
  std::optional<std::string> subtitle_url_;
  PlayerState state_;
  uint32_t last_props_ms_ = 0;
  bool props_read_ = false;
};

}  // namespace zerofin::player
