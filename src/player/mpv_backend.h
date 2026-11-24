#pragma once

#include "player/backend.h"

struct mpv_handle;
struct mpv_render_context;

namespace zerofin::player {

// Plays with libmpv and its software render API. The settings come from the spike
// (spike-results.md): hardware decode with v4l2m2m-copy, and a scale filter so the RGB565
// conversion only touches a small picture.
class MpvBackend : public PlayerBackend {
 public:
  MpvBackend() = default;
  ~MpvBackend() override;
  MpvBackend(const MpvBackend&) = delete;
  MpvBackend& operator=(const MpvBackend&) = delete;

  bool start(const StartParams& params) override;
  PlayerState poll(uint32_t now_ms) override;
  bool render(uint8_t* rgb565, size_t stride) override;
  void set_paused(bool paused) override;
  void seek_relative(double seconds) override;
  void set_volume(int percent) override;
  void stop() override;

 private:
  bool create_handle();
  void drain_events();
  void refresh_properties(uint32_t now_ms);
  void fail(const std::string& what);

  mpv_handle* mpv_ = nullptr;
  mpv_render_context* render_ = nullptr;
  std::optional<std::string> subtitle_url_;
  PlayerState state_;
  uint32_t last_props_ms_ = 0;
  bool props_read_ = false;
};

}  // namespace zerofin::player
