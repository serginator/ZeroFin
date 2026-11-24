#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui/async.h"
#include "ui_core/keys.h"

namespace zerofin::platform {
class Platform;
}

namespace zerofin::app {

// Development aid. Plays a list of steps so a screen can be checked without a person at the
// keyboard. Set ZEROFIN_SCRIPT, for example "wait:500;key:down;key:enter;idle:5000;shot:detail;quit".
//   wait:MS      pause
//   idle:MS      wait until no network work is running (at most MS)
//   key:NAME     up down left right enter esc tab backspace delete home end pageup pagedown
//                playpause prev next voldown volup
//   text:STRING  type the characters
//   shot:NAME    save $ZEROFIN_SHOT_DIR/NAME.ppm
//   quit         end the program
class ScriptRunner {
 public:
  ScriptRunner(const std::string& script, std::string shot_dir);

  // Adds key events to `keys`. Returns false when the script is finished.
  bool tick(uint32_t now_ms, const Async& async, platform::Platform& platform, std::vector<ui::KeyEvent>& keys);

 private:
  struct Step {
    std::string kind;
    std::string arg;
  };
  std::vector<Step> steps_;
  size_t next_ = 0;
  uint32_t resume_at_ = 0;
  uint32_t idle_deadline_ = 0;
  bool waiting_idle_ = false;
  bool started_ = false;
  std::string shot_dir_;
};

}  // namespace zerofin::app
