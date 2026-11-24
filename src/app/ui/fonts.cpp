#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "ui/theme.h"

#ifndef ZEROFIN_ASSETS_SOURCE_DIR
#define ZEROFIN_ASSETS_SOURCE_DIR "assets"
#endif

namespace zerofin::app {
namespace {

std::string find_font_file(const std::string& name) {
  std::vector<std::filesystem::path> roots;
  if (const char* env = std::getenv("ZEROFIN_ASSETS"); env && *env) roots.emplace_back(env);
  roots.emplace_back(ZEROFIN_ASSETS_SOURCE_DIR);
  roots.emplace_back("/usr/share/zerofin");
  for (const auto& root : roots) {
    const auto candidate = root / "fonts" / name;
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec)) return candidate.string();
  }
  return {};
}

const lv_font_t* load(const std::string& file, uint32_t size, const lv_font_t* fallback) {
  const std::string path = find_font_file(file);
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
