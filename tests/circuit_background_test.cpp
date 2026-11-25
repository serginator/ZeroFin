#include <doctest/doctest.h>

#include <algorithm>

#include "ui_core/circuit_background.h"

using namespace zerofin;

namespace {
constexpr int kW = 320, kH = 170, kTop = 20;

uint16_t base565() {
  const uint32_t rgb = ui::kCircuitBaseRgb;
  return static_cast<uint16_t>((((rgb >> 16) & 0xFF) >> 3 << 11) | ((((rgb >> 8) & 0xFF) >> 2) << 5) | ((rgb & 0xFF) >> 3));
}
}  // namespace

TEST_CASE("the picture has one pixel for every position") {
  CHECK(ui::render_circuit_background(kW, kH, kTop).size() == static_cast<size_t>(kW) * kH);
  CHECK(ui::render_circuit_background(0, kH, kTop).empty());
  CHECK(ui::render_circuit_background(kW, -1, kTop).empty());
}

TEST_CASE("the same size always gives the same picture") {
  CHECK(ui::render_circuit_background(kW, kH, kTop) == ui::render_circuit_background(kW, kH, kTop));
}

TEST_CASE("the rows under the title bar stay plain") {
  const auto px = ui::render_circuit_background(kW, kH, kTop);
  for (int y = 0; y < kTop; ++y) {
    for (int x = 0; x < kW; ++x) CHECK(px[static_cast<size_t>(y) * kW + x] == base565());
  }
}

TEST_CASE("the traces are drawn, but stay faint") {
  const auto px = ui::render_circuit_background(kW, kH, kTop);
  const auto plain = static_cast<size_t>(std::count(px.begin(), px.end(), base565()));
  CHECK(plain < px.size());                 // there is something drawn
  CHECK(plain > px.size() * 8 / 10);        // and most of the picture is still the plain base
  for (uint16_t p : px) {
    const int r = (p >> 11) << 3, g = ((p >> 5) & 0x3F) << 2, b = (p & 0x1F) << 3;
    CHECK(std::max({r, g, b}) <= 170);      // light text (about 0x8B or brighter) keeps its contrast
  }
}

TEST_CASE("a tiny picture does not crash") {
  CHECK(ui::render_circuit_background(10, 10, 20).size() == 100);
}
