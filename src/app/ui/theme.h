#pragma once

#include "lvgl.h"

namespace zerofin::app {

// Screen geometry. The panel is 320x170. A 20 px title bar leaves 150 px for content.
constexpr int kScreenW = 320;
constexpr int kScreenH = 170;
constexpr int kTitleH = 20;
constexpr int kRowH = 25;
constexpr int kContentH = kScreenH - kTitleH;

namespace color {
constexpr uint32_t kBackground = 0x0F1419;
constexpr uint32_t kBar = 0x1B2530;
constexpr uint32_t kText = 0xE8EAED;
constexpr uint32_t kDim = 0x8B97A3;
constexpr uint32_t kAccent = 0x00A4DC;  // Jellyfin blue
constexpr uint32_t kSelected = 0x1D3B52;
constexpr uint32_t kGood = 0x4CAF50;
constexpr uint32_t kWarn = 0xFFB300;
constexpr uint32_t kError = 0xEF5350;
}  // namespace color

struct Fonts {
  const lv_font_t* small = nullptr;   // 12 px
  const lv_font_t* normal = nullptr;  // 14 px
  const lv_font_t* bold = nullptr;    // 14 px bold
  const lv_font_t* big = nullptr;     // 30 px bold, for the Quick Connect code
};

// Loads DejaVu Sans Condensed with FreeType. Falls back to built-in Montserrat when the
// font files are missing. Must be called after lv_init().
Fonts load_fonts();

}  // namespace zerofin::app
