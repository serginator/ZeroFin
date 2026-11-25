#pragma once

#include <cstdint>
#include <vector>

#include "lvgl.h"

namespace zerofin::app {

// The picture behind every screen with a title bar: faint neon purple and blue circuit traces
// (drawn once at start by ui_core). LVGL shows the pixels as they are, with no decoding.
class Background {
 public:
  Background();
  Background(const Background&) = delete;
  Background& operator=(const Background&) = delete;

  const lv_image_dsc_t* image() const { return &dsc_; }

 private:
  std::vector<uint16_t> pixels_;
  lv_image_dsc_t dsc_{};
};

}  // namespace zerofin::app
