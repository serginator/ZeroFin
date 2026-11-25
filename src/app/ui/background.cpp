#include "ui/background.h"

#include "ui/theme.h"
#include "ui_core/circuit_background.h"

namespace zerofin::app {

Background::Background() : pixels_(ui::render_circuit_background(kScreenW, kScreenH, kTitleH)) {
  dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
  dsc_.header.w = kScreenW;
  dsc_.header.h = kScreenH;
  dsc_.header.stride = kScreenW * 2;
  dsc_.data = reinterpret_cast<const uint8_t*>(pixels_.data());
  dsc_.data_size = static_cast<uint32_t>(pixels_.size() * sizeof(uint16_t));
}

}  // namespace zerofin::app
