#pragma once

namespace zerofin::ui {

enum class OutputChange { None, ToMonitor, ToLcd };

// Decides when a playing video must move between the LCD and a monitor. Pure logic: the caller checks
// every couple of seconds and passes what it sees.
//  - A pulled cable (playing on the monitor, monitor not wanted any more) moves the video to the LCD at once.
//  - A new monitor (playing on the LCD, monitor wanted) must be seen on two checks in a row, because the monitor
//    reports "connected" a moment before it is ready.
class OutputWatch {
 public:
  OutputChange check(bool on_monitor, bool wants_monitor);

 private:
  int wanted_streak_ = 0;
};

}  // namespace zerofin::ui
