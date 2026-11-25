#pragma once

#include <memory>
#include <string>

#include "lvgl.h"

namespace zerofin::app {

// The banner that covers the whole panel when the app starts. It sits on the top layer, so the first
// screen can build and load under it. The caller decides when to close it (a key, or a time limit).
class Splash {
 public:
  // Returns null when the banner file is missing or is not a JPEG that LVGL can decode.
  static std::unique_ptr<Splash> create(lv_obj_t* layer);
  ~Splash();
  Splash(const Splash&) = delete;
  Splash& operator=(const Splash&) = delete;

 private:
  Splash() = default;

  lv_obj_t* root_ = nullptr;
  std::string bytes_;
  lv_image_dsc_t dsc_{};
};

}  // namespace zerofin::app
