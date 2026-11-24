#include "script_runner.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "lvgl.h"
#include "platform/platform.h"

namespace zerofin::app {
namespace {

ui::Key key_from_name(const std::string& n) {
  if (n == "up") return ui::Key::Up;
  if (n == "down") return ui::Key::Down;
  if (n == "left") return ui::Key::Left;
  if (n == "right") return ui::Key::Right;
  if (n == "enter") return ui::Key::Enter;
  if (n == "esc") return ui::Key::Esc;
  if (n == "tab") return ui::Key::Tab;
  if (n == "backspace") return ui::Key::Backspace;
  if (n == "delete") return ui::Key::Delete;
  if (n == "home") return ui::Key::Home;
  if (n == "end") return ui::Key::End;
  if (n == "pageup") return ui::Key::PageUp;
  if (n == "pagedown") return ui::Key::PageDown;
  if (n == "playpause") return ui::Key::PlayPause;
  if (n == "prev") return ui::Key::Prev;
  if (n == "next") return ui::Key::Next;
  if (n == "voldown") return ui::Key::VolumeDown;
  if (n == "volup") return ui::Key::VolumeUp;
  return ui::Key::None;
}

}  // namespace

ScriptRunner::ScriptRunner(const std::string& script, std::string shot_dir) : shot_dir_(std::move(shot_dir)) {
  std::stringstream ss(script);
  std::string part;
  while (std::getline(ss, part, ';')) {
    if (part.empty()) continue;
    const auto colon = part.find(':');
    steps_.push_back({part.substr(0, colon), colon == std::string::npos ? "" : part.substr(colon + 1)});
  }
}

bool ScriptRunner::tick(uint32_t now, const Async& async, platform::Platform& platform,
                        std::vector<ui::KeyEvent>& keys) {
  if (!started_) {
    started_ = true;
    resume_at_ = now;
  }
  if (static_cast<int32_t>(now - resume_at_) < 0) return true;

  while (next_ < steps_.size()) {
    const Step& s = steps_[next_];
    if (s.kind == "wait") {
      resume_at_ = now + static_cast<uint32_t>(std::atoi(s.arg.c_str()));
      ++next_;
      return true;
    }
    if (s.kind == "idle") {
      if (!waiting_idle_) {
        waiting_idle_ = true;
        idle_deadline_ = now + static_cast<uint32_t>(std::atoi(s.arg.c_str()));
      }
      if (async.busy() && static_cast<int32_t>(now - idle_deadline_) < 0) return true;
      waiting_idle_ = false;
      resume_at_ = now + 150;  // let the screen draw what arrived
      ++next_;
      return true;
    }
    if (s.kind == "key") {
      keys.push_back(ui::KeyEvent{key_from_name(s.arg), 0, false});
      resume_at_ = now + 150;
      ++next_;
      return true;
    }
    if (s.kind == "text") {
      for (char c : s.arg) keys.push_back(ui::KeyEvent{ui::Key::Char, c, false});
      resume_at_ = now + 150;
      ++next_;
      return true;
    }
    if (s.kind == "shot") {
      lv_refr_now(nullptr);
      const std::string path = shot_dir_ + "/" + s.arg + ".ppm";
      if (!platform.save_snapshot(path)) std::fprintf(stderr, "cannot save %s\n", path.c_str());
      ++next_;
      continue;
    }
    if (s.kind == "quit") return false;
    std::fprintf(stderr, "unknown script step: %s\n", s.kind.c_str());
    ++next_;
  }
  return false;
}

}  // namespace zerofin::app
