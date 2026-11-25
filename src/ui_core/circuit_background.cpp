#include "ui_core/circuit_background.h"

#include <algorithm>
#include <cmath>

namespace zerofin::ui {
namespace {

struct Color {
  float r, g, b;
  float gain = 1.0f;  // the eye sees purple as dimmer than blue, so purple lines are a little stronger
};

constexpr Color kBase = {static_cast<float>((kCircuitBaseRgb >> 16) & 0xFF), static_cast<float>((kCircuitBaseRgb >> 8) & 0xFF),
                         static_cast<float>(kCircuitBaseRgb & 0xFF)};
constexpr Color kPurple = {0xB0, 0x4F, 0xFF, 1.25f};
constexpr Color kBlue = {0x00, 0xC8, 0xFF};

constexpr float kLineAlpha = 0.34f;
constexpr float kGlowAlpha = 0.09f;
constexpr float kNodeAlpha = 0.45f;
constexpr int kGrid = 8;
constexpr int kTraces = 16;

// A small generator with a fixed seed, so the picture never changes between runs.
struct Rng {
  uint32_t state = 0x2F6E2B1u;
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
  }
  int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1)); }
};

class Canvas {
 public:
  Canvas(int w, int h, int top) : w_(w), h_(h), top_(top), px_(static_cast<size_t>(w) * h, kBase) {}

  void blend(int x, int y, const Color& c, float alpha) {
    if (x < 0 || y < top_ || x >= w_ || y >= h_) return;  // nothing is drawn under the title bar
    Color& p = px_[static_cast<size_t>(y) * w_ + x];
    alpha *= c.gain;
    // Keep the strongest value, so where two traces cross the pixel is no brighter than the stronger one.
    p.r = std::max(p.r, kBase.r + (c.r - kBase.r) * alpha);
    p.g = std::max(p.g, kBase.g + (c.g - kBase.g) * alpha);
    p.b = std::max(p.b, kBase.b + (c.b - kBase.b) * alpha);
  }

  // A one pixel line (only horizontal, vertical and 45 degree lines are used) with a faint glow around it.
  void line(int x0, int y0, int x1, int y1, const Color& c) {
    const int dx = (x1 > x0) - (x1 < x0);
    const int dy = (y1 > y0) - (y1 < y0);
    for (int x = x0, y = y0;; x += dx, y += dy) {
      blend(x + 1, y, c, kGlowAlpha);
      blend(x - 1, y, c, kGlowAlpha);
      blend(x, y + 1, c, kGlowAlpha);
      blend(x, y - 1, c, kGlowAlpha);
      blend(x, y, c, kLineAlpha);
      if (x == x1 && y == y1) break;
    }
  }

  // A ring (the pad at the end of a trace), with an optional dot in the middle.
  void pad(int cx, int cy, const Color& c, bool filled) {
    static const int kRing[][2] = {{3, 0}, {3, 1}, {2, 2}, {1, 3}, {0, 3}};
    for (const auto& o : kRing) {
      for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
          blend(cx + sx * o[0], cy + sy * o[1], c, kNodeAlpha);
        }
      }
    }
    if (filled) {
      blend(cx, cy, c, kNodeAlpha);
      blend(cx + 1, cy, c, kNodeAlpha * 0.5f);
      blend(cx - 1, cy, c, kNodeAlpha * 0.5f);
      blend(cx, cy + 1, c, kNodeAlpha * 0.5f);
      blend(cx, cy - 1, c, kNodeAlpha * 0.5f);
    }
  }

  const Color& at(int x, int y) const { return px_[static_cast<size_t>(y) * w_ + x]; }

 private:
  int w_, h_, top_;
  std::vector<Color> px_;
};

// One trace: straight, then a 45 degree jog to the side, then straight again, then a pad.
void draw_trace(Canvas& canvas, Rng& rng, int width, int height, int top_margin) {
  static const int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  const Color& color = rng.range(0, 1) ? kPurple : kBlue;
  int x = rng.range(1, (width - 2 * kGrid) / kGrid) * kGrid;
  int y = top_margin + kGrid + rng.range(0, (height - top_margin - 2 * kGrid) / kGrid) * kGrid;
  const int* d = kDirs[rng.range(0, 3)];
  const int side = rng.range(0, 1) ? 1 : -1;
  const int px = -d[1] * side;  // the sideways direction of the jog
  const int py = d[0] * side;

  const int len1 = rng.range(2, 6) * kGrid;
  const int jog = rng.range(1, 3) * kGrid;
  const int len2 = rng.range(1, 5) * kGrid;

  canvas.blend(x, y, color, kNodeAlpha);
  int nx = x + d[0] * len1, ny = y + d[1] * len1;
  canvas.line(x, y, nx, ny, color);
  x = nx;
  y = ny;
  nx = x + (d[0] + px) * jog;
  ny = y + (d[1] + py) * jog;
  canvas.line(x, y, nx, ny, color);
  x = nx;
  y = ny;
  nx = x + d[0] * len2;
  ny = y + d[1] * len2;
  canvas.line(x, y, nx, ny, color);
  canvas.pad(nx, ny, color, rng.range(0, 1) == 1);
}

// 4x4 ordered dither matrix, values 0..15.
constexpr int kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

uint16_t to_rgb565(const Color& c, int x, int y) {
  // floor(value / step + threshold) rounds the value to the nearest level, and the threshold pattern
  // spreads the rounding error, so the faint glow has no hard steps.
  const float t = (kBayer[y & 3][x & 3] + 0.5f) / 16.0f;
  auto level = [t](float v, float step, int max) {
    return std::clamp(static_cast<int>(std::floor(v / step + t)), 0, max);
  };
  const int r = level(c.r, 8.0f, 31);
  const int g = level(c.g, 4.0f, 63);
  const int b = level(c.b, 8.0f, 31);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

}  // namespace

std::vector<uint16_t> render_circuit_background(int width, int height, int top_margin) {
  std::vector<uint16_t> out;
  if (width <= 0 || height <= 0) return out;
  Canvas canvas(width, height, top_margin);
  if (width > 4 * kGrid && height - top_margin > 4 * kGrid) {
    Rng rng;
    for (int i = 0; i < kTraces; ++i) draw_trace(canvas, rng, width, height, top_margin);
  }
  out.reserve(static_cast<size_t>(width) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) out.push_back(to_rgb565(canvas.at(x, y), x, y));
  }
  return out;
}

}  // namespace zerofin::ui
