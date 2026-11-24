#include "ui_core/player_timers.h"

namespace zerofin::ui {

void ReportScheduler::start(uint32_t now_ms) {
  last_ms_ = now_ms;
  last_paused_ = false;
}

bool ReportScheduler::progress_due(uint32_t now_ms, bool paused) {
  if (paused != last_paused_) {
    last_paused_ = paused;
    last_ms_ = now_ms;
    return true;
  }
  if (now_ms - last_ms_ >= interval_ms_) {
    last_ms_ = now_ms;
    return true;
  }
  return false;
}

void Countdown::start(uint32_t now_ms, uint32_t duration_ms) {
  active_ = true;
  start_ms_ = now_ms;
  duration_ms_ = duration_ms;
}

bool Countdown::done(uint32_t now_ms) const { return active_ && now_ms - start_ms_ >= duration_ms_; }

int Countdown::seconds_left(uint32_t now_ms) const {
  if (!active_) return 0;
  const uint32_t elapsed = now_ms - start_ms_;
  if (elapsed >= duration_ms_) return 0;
  return static_cast<int>((duration_ms_ - elapsed + 999) / 1000);
}

bool reached_end(double position_s, double duration_s, double tolerance_s) {
  if (duration_s <= 0) return true;
  return position_s >= duration_s - tolerance_s;
}

double restart_position(bool started, double position_s, double planned_start_s) {
  return started ? position_s : planned_start_s;
}

}  // namespace zerofin::ui
