#pragma once

#include <cstdint>
#include <string>

namespace zerofin::platform {

struct Rgb888 {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Expands one RGB565 pixel to RGB888 by replicating the top bits into the low bits.
inline Rgb888 rgb565_to_rgb888(uint16_t p) {
  const unsigned r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
  return Rgb888{static_cast<uint8_t>((r << 3) | (r >> 2)), static_cast<uint8_t>((g << 2) | (g >> 4)),
                static_cast<uint8_t>((b << 3) | (b >> 2))};
}

// Writes a binary PPM (P6). `rgb888` holds w*h pixels of 3 bytes each. Returns false on I/O error.
bool write_ppm(const std::string& path, int w, int h, const uint8_t* rgb888);

}  // namespace zerofin::platform
