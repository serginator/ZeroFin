#pragma once

#include <cstdint>

namespace zerofin::ui {

// Decides when the player sends a progress report to the server. Pure logic: the caller
// passes the time.
class ReportScheduler {
 public:
  explicit ReportScheduler(uint32_t interval_ms = 10000) : interval_ms_(interval_ms) {}

  // The first frame was shown.
  void start(uint32_t now_ms);

  // True once per interval while playing. Also true at once when `paused` differs from the
  // value of the last call.
  bool progress_due(uint32_t now_ms, bool paused);

 private:
  uint32_t interval_ms_;
  uint32_t last_ms_ = 0;
  bool last_paused_ = false;
};

// The "next episode plays in N s" countdown.
class Countdown {
 public:
  void start(uint32_t now_ms, uint32_t duration_ms = 10000);
  void cancel() { active_ = false; }
  bool active() const { return active_; }

  // Rounds up: 10 at the start, 1 in the last second, 0 when done.
  int seconds_left(uint32_t now_ms) const;
  // False when the countdown is not active.
  bool done(uint32_t now_ms) const;

 private:
  bool active_ = false;
  uint32_t start_ms_ = 0;
  uint32_t duration_ms_ = 0;
};

// An end of file counts as the real end of the video only when the position is within
// `tolerance_s` of the length. A dropped connection can look like an end of file. An unknown
// length (0) trusts the end of file.
bool reached_end(double position_s, double duration_s, double tolerance_s = 10);

// Where a reload starts. Before the first frame the position is still 0, so the planned start
// (the resume point) stays.
double restart_position(bool started, double position_s, double planned_start_s);

}  // namespace zerofin::ui
