#pragma once

#include "player/backend.h"

namespace zerofin::player {

constexpr double kDemoDurationSeconds = 5760.0;  // 1 h 36 min

// A backend with no decoder. It draws a moving test pattern and keeps a fake clock. The
// simulator and the device use it with ZEROFIN_DEMO=1, so the whole player UI can run
// without a server or libmpv.
class DemoBackend : public PlayerBackend {
 public:
  bool start(const StartParams& params) override;
  PlayerState poll(uint32_t now_ms) override;
  bool render(uint8_t* rgb565, size_t stride) override;
  void set_paused(bool paused) override { paused_ = paused; }
  void seek_relative(double seconds) override;
  void set_volume(int) override {}
  void set_audio_sink(AudioSink) override {}
  void stop() override { running_ = false; }

 private:
  bool running_ = false;
  bool paused_ = false;
  bool polled_ = false;
  bool ended_ = false;
  bool rendered_once_ = false;
  double position_ = 0;
  double duration_ = kDemoDurationSeconds;  // ZEROFIN_DEMO_DURATION (seconds) overrides it
  uint32_t last_poll_ms_ = 0;
  uint32_t last_render_ms_ = 0;
  uint32_t frame_ = 0;
};

}  // namespace zerofin::player
