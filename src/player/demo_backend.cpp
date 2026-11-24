#include "player/demo_backend.h"

#include <algorithm>
#include <cstdlib>

namespace zerofin::player {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 170;
constexpr uint32_t kFrameIntervalMs = 40;  // 25 fps

uint16_t rgb565(int r, int g, int b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

}  // namespace

bool DemoBackend::start(const StartParams& params) {
  running_ = true;
  paused_ = false;
  polled_ = false;
  ended_ = false;
  rendered_once_ = false;
  frame_ = 0;
  duration_ = kDemoDurationSeconds;
  if (const char* env = std::getenv("ZEROFIN_DEMO_DURATION")) {
    const double seconds = std::atof(env);
    if (seconds > 0) duration_ = seconds;
  }
  position_ = std::clamp(params.start_seconds, 0.0, duration_);
  return true;
}

PlayerState DemoBackend::poll(uint32_t now_ms) {
  if (running_ && polled_ && !paused_ && !ended_) {
    position_ += static_cast<double>(now_ms - last_poll_ms_) / 1000.0;
  }
  last_poll_ms_ = now_ms;
  polled_ = true;
  if (running_ && position_ >= duration_) {
    position_ = duration_;
    ended_ = true;
  }
  PlayerState s;
  s.started = running_;
  s.paused = paused_;
  s.position_s = position_;
  s.duration_s = duration_;
  s.ended = ended_ ? EndReason::Eof : EndReason::None;
  return s;
}

void DemoBackend::seek_relative(double seconds) {
  position_ = std::max(0.0, position_ + seconds);
  if (position_ >= duration_) {
    position_ = duration_;
    ended_ = true;
  }
}

bool DemoBackend::render(uint8_t* pixels, size_t stride) {
  if (!running_) return false;
  if (rendered_once_ && last_poll_ms_ - last_render_ms_ < kFrameIntervalMs) return false;
  rendered_once_ = true;
  last_render_ms_ = last_poll_ms_;
  ++frame_;
  // A gradient that shifts with the position, and a white bar that grows with the frame counter.
  const int shift = static_cast<int>(position_ * 20.0) % 256;
  const int bar = static_cast<int>(frame_ % kWidth);
  for (int y = 0; y < kHeight; ++y) {
    auto* row = reinterpret_cast<uint16_t*>(pixels + static_cast<size_t>(y) * stride);
    for (int x = 0; x < kWidth; ++x) {
      if (y >= kHeight - 6 && x <= bar) {
        row[x] = rgb565(255, 255, 255);
      } else {
        row[x] = rgb565((x + shift) & 0xFF, y * 255 / kHeight, 255 - ((x + shift) & 0xFF));
      }
    }
  }
  return true;
}

}  // namespace zerofin::player
