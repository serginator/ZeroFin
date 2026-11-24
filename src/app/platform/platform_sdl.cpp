// Desktop simulator: draws the 320x170 LVGL display into an SDL window (3x zoom).
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "platform/platform.h"
#include "platform/platform_common.h"
#include "ui/theme.h"

namespace zerofin::platform {

namespace {

constexpr int kW = app::kScreenW;
constexpr int kH = app::kScreenH;
constexpr int kZoom = 3;

class SdlPlatform : public Platform {
 public:
  SdlPlatform() {
    if (std::getenv("ZEROFIN_HEADLESS")) setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window_ = SDL_CreateWindow("ZeroFin simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kW * kZoom,
                               kH * kZoom, SDL_WINDOW_SHOWN);
    if (window_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (renderer_) {
      SDL_RenderSetLogicalSize(renderer_, kW, kH);
      texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    }
    pixels_.assign(static_cast<size_t>(kW) * kH, 0xFF000000u);
    buffer_.resize(static_cast<size_t>(kW) * kH * 2);

    disp_ = lv_display_create(kW, kH);
    lv_display_set_color_format(disp_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp_, buffer_.data(), nullptr, static_cast<uint32_t>(buffer_.size()),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp_, this);
    lv_display_set_flush_cb(disp_, &SdlPlatform::flush);
    SDL_StartTextInput();
  }

  ~SdlPlatform() override {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
  }

  lv_display_t* display() override { return disp_; }

  bool poll(std::vector<ui::KeyEvent>& out) override {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          return false;
        case SDL_KEYDOWN:
          if (auto k = map_key(e.key.keysym.sym); k != ui::Key::None) {
            out.push_back(ui::KeyEvent{k, 0, e.key.repeat != 0});
          }
          break;
        case SDL_TEXTINPUT:
          for (const char* p = e.text.text; *p; ++p) {
            if (*p >= 32 && *p <= 126) out.push_back(ui::KeyEvent{ui::Key::Char, *p, false});
          }
          break;
        default:
          break;
      }
    }
    return true;
  }

  bool save_snapshot(const std::string& path) override {
    std::vector<uint8_t> rgb;
    rgb.reserve(pixels_.size() * 3);
    for (uint32_t px : pixels_) {
      rgb.push_back(static_cast<uint8_t>(px >> 16));
      rgb.push_back(static_cast<uint8_t>(px >> 8));
      rgb.push_back(static_cast<uint8_t>(px));
    }
    return write_ppm(path, kW, kH, rgb.data());
  }

 private:
  static ui::Key map_key(SDL_Keycode sym) {
    switch (sym) {
      case SDLK_UP: return ui::Key::Up;
      case SDLK_DOWN: return ui::Key::Down;
      case SDLK_LEFT: return ui::Key::Left;
      case SDLK_RIGHT: return ui::Key::Right;
      case SDLK_RETURN:
      case SDLK_KP_ENTER: return ui::Key::Enter;
      case SDLK_ESCAPE: return ui::Key::Esc;
      case SDLK_TAB: return ui::Key::Tab;
      case SDLK_BACKSPACE: return ui::Key::Backspace;
      case SDLK_DELETE: return ui::Key::Delete;
      case SDLK_HOME: return ui::Key::Home;
      case SDLK_END: return ui::Key::End;
      case SDLK_PAGEUP: return ui::Key::PageUp;
      case SDLK_PAGEDOWN: return ui::Key::PageDown;
      // Media keys of the CP0 (fn+Q, fn+W, fn+E, fn+S, fn+D) on function keys.
      case SDLK_F1: return ui::Key::PlayPause;
      case SDLK_F2: return ui::Key::Prev;
      case SDLK_F3: return ui::Key::Next;
      case SDLK_F4: return ui::Key::VolumeDown;
      case SDLK_F5: return ui::Key::VolumeUp;
      default: return ui::Key::None;
    }
  }

  static void flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto* self = static_cast<SdlPlatform*>(lv_display_get_user_data(disp));
    const int w = lv_area_get_width(area);
    const int h = lv_area_get_height(area);
    const auto* src = reinterpret_cast<const uint16_t*>(px_map);
    for (int y = 0; y < h; ++y) {
      uint32_t* dst = &self->pixels_[static_cast<size_t>(area->y1 + y) * kW + static_cast<size_t>(area->x1)];
      for (int x = 0; x < w; ++x) {
        const uint16_t p = src[y * w + x];
        const Rgb888 c = rgb565_to_rgb888(p);
        dst[x] = 0xFF000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
      }
    }
    if (lv_display_flush_is_last(disp) && self->texture_) {
      SDL_UpdateTexture(self->texture_, nullptr, self->pixels_.data(), kW * 4);
      SDL_RenderClear(self->renderer_);
      SDL_RenderCopy(self->renderer_, self->texture_, nullptr, nullptr);
      SDL_RenderPresent(self->renderer_);
    }
    lv_display_flush_ready(disp);
  }

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  lv_display_t* disp_ = nullptr;
  std::vector<uint32_t> pixels_;
  std::vector<uint8_t> buffer_;
};

}  // namespace

std::unique_ptr<Platform> create_platform() { return std::make_unique<SdlPlatform>(); }

}  // namespace zerofin::platform
