#pragma once

#include <cstdint>
#include <vector>

namespace zerofin::ui {

// Draws thin neon purple and neon blue "circuit traces" on a dark base and returns the picture as
// width*height RGB565 pixels, row by row. The traces are faint, so text on top stays easy to read.
// The rows above `top_margin` stay plain (a title bar covers them). The same size always gives the same picture.
std::vector<uint16_t> render_circuit_background(int width, int height, int top_margin);

// The plain colour of the picture (an RGB565 value that is exact in 8 bits per channel).
constexpr uint32_t kCircuitBaseRgb = 0x100C20;

}  // namespace zerofin::ui
