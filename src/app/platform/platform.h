#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "lvgl.h"
#include "ui_core/keys.h"

namespace zerofin::platform {

// The display and the keyboard. The simulator uses SDL. The device uses the Linux
// framebuffer and evdev.
class Platform {
 public:
  virtual ~Platform() = default;
  virtual lv_display_t* display() = 0;
  // Appends new key events. Returns false when the user asked to quit.
  virtual bool poll(std::vector<ui::KeyEvent>& out) = 0;
  // Saves the last frame as a binary PPM file. Returns false when unsupported.
  virtual bool save_snapshot(const std::string& /*ppm_path*/) { return false; }
};

std::unique_ptr<Platform> create_platform();

uint32_t tick_ms();

}  // namespace zerofin::platform
