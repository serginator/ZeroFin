#include <cstdio>
#include <string>

#include "ui/assets.h"
#include "ui/theme.h"

namespace zerofin::app {
namespace {

const lv_font_t* load(const std::string& file, uint32_t size, const lv_font_t* fallback) {
  const std::string path = find_asset("fonts/" + file);
  if (path.empty()) {
    std::fprintf(stderr, "font %s not found, using the built-in font\n", file.c_str());
    return fallback;
  }
  lv_font_t* font = lv_freetype_font_create(path.c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, size,
                                            LV_FREETYPE_FONT_STYLE_NORMAL);
  if (!font) {
    std::fprintf(stderr, "cannot load %s\n", path.c_str());
    return fallback;
  }
  font->fallback = fallback;
  return font;
}

}  // namespace

Fonts load_fonts() {
  Fonts f;
  f.small = load("DejaVuSansCondensed.ttf", 12, &lv_font_montserrat_12);
  f.normal = load("DejaVuSansCondensed.ttf", 14, &lv_font_montserrat_14);
  f.bold = load("DejaVuSansCondensed-Bold.ttf", 14, &lv_font_montserrat_14);
  f.big = load("DejaVuSansCondensed-Bold.ttf", 30, &lv_font_montserrat_16);
  return f;
}

}  // namespace zerofin::app
