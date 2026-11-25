#include "ui/splash.h"

#include <fstream>
#include <iterator>
#include <memory>

#include "jellyfin/image_fetch.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/misc/cache/instance/lv_image_header_cache.h"
#include "ui/assets.h"
#include "ui/theme.h"

namespace zerofin::app {

std::unique_ptr<Splash> Splash::create(lv_obj_t* layer) {
  const std::string path = find_asset("images/banner-splash.jpg");
  if (path.empty()) return nullptr;
  std::ifstream in(path, std::ios::binary);

  std::unique_ptr<Splash> splash(new Splash());
  splash->bytes_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  // The same rule as for posters: LVGL decodes only a baseline JPEG.
  const auto size = jf::jpeg_size(splash->bytes_);
  if (!size) return nullptr;
  splash->dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  splash->dsc_.header.cf = LV_COLOR_FORMAT_RAW;
  splash->dsc_.header.w = static_cast<uint32_t>(size->first);
  splash->dsc_.header.h = static_cast<uint32_t>(size->second);
  splash->dsc_.data = reinterpret_cast<const uint8_t*>(splash->bytes_.data());
  splash->dsc_.data_size = static_cast<uint32_t>(splash->bytes_.size());

  lv_obj_t* root = lv_obj_create(layer);
  lv_obj_remove_style_all(root);
  lv_obj_set_size(root, kScreenW, kScreenH);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* img = lv_image_create(root);
  lv_image_set_src(img, &splash->dsc_);
  lv_obj_center(img);
  splash->root_ = root;
  return splash;
}

Splash::~Splash() {
  if (root_) lv_obj_delete(root_);
  // LVGL keys its decoded-image cache by the address of the descriptor.
  lv_image_cache_drop(&dsc_);
  lv_image_header_cache_drop(&dsc_);
}

}  // namespace zerofin::app
