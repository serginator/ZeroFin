#include "ui_core/output_watch.h"

namespace zerofin::ui {

OutputChange OutputWatch::check(bool on_monitor, bool wants_monitor) {
  if (on_monitor) {
    wanted_streak_ = 0;
    return wants_monitor ? OutputChange::None : OutputChange::ToLcd;
  }
  if (!wants_monitor) {
    wanted_streak_ = 0;
    return OutputChange::None;
  }
  if (++wanted_streak_ < 2) return OutputChange::None;
  wanted_streak_ = 0;
  return OutputChange::ToMonitor;
}

}  // namespace zerofin::ui
