// Device: LVGL framebuffer display and Linux evdev keyboard input.
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "platform/platform.h"
#include "platform/platform_common.h"
#include "ui/theme.h"

namespace zerofin::platform {

namespace {

bool has_key(int fd, int code) {
  unsigned long bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;
  const int per = static_cast<int>(sizeof(unsigned long) * 8);
  return (bits[code / per] & (1UL << (code % per))) != 0;
}

class FbPlatform : public Platform {
 public:
  FbPlatform() {
    const char* env_fb = std::getenv("ZEROFIN_FB");
    std::string fb = env_fb && *env_fb ? env_fb : (std::filesystem::exists("/dev/fb_lcd") ? "/dev/fb_lcd" : "/dev/fb0");
    fb_path_ = fb;
    disp_ = lv_linux_fbdev_create();
    if (!disp_ || lv_linux_fbdev_set_file(disp_, fb.c_str()) != LV_RESULT_OK) {
      std::fprintf(stderr, "cannot open framebuffer %s\n", fb.c_str());
      disp_ = nullptr;
    }
    log_keys_ = std::getenv("ZEROFIN_KEYLOG") != nullptr;
    open_keyboards();
  }

  ~FbPlatform() override {
    for (int fd : fds_) close(fd);
  }

  lv_display_t* display() override { return disp_; }

  // Reads the real panel contents from the framebuffer (RGB565, 320x170) and writes a PPM.
  bool save_snapshot(const std::string& path) override {
    const int fd = open(fb_path_.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    constexpr int kW = app::kScreenW;
    constexpr int kH = app::kScreenH;
    std::vector<uint16_t> px(static_cast<size_t>(kW) * kH);
    const ssize_t want = static_cast<ssize_t>(px.size() * sizeof(uint16_t));
    const ssize_t got = pread(fd, px.data(), static_cast<size_t>(want), 0);
    close(fd);
    if (got != want) return false;
    std::vector<uint8_t> rgb;
    rgb.reserve(px.size() * 3);
    for (uint16_t p : px) {
      const Rgb888 c = rgb565_to_rgb888(p);
      rgb.push_back(c.r);
      rgb.push_back(c.g);
      rgb.push_back(c.b);
    }
    return write_ppm(path, kW, kH, rgb.data());
  }

  bool poll(std::vector<ui::KeyEvent>& out) override {
    for (int fd : fds_) {
      input_event ev{};
      while (read(fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
        if (ev.type != EV_KEY) continue;
        handle(ev.code, ev.value, out);
      }
    }
    return true;
  }

 private:
  void open_keyboards() {
    DIR* dir = opendir("/dev/input");
    if (!dir) {
      std::fprintf(stderr, "cannot open /dev/input\n");
      return;
    }
    while (dirent* entry = readdir(dir)) {
      if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
      const std::string path = std::string("/dev/input/") + entry->d_name;
      const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (fd < 0) continue;
      if (has_key(fd, KEY_ENTER) || has_key(fd, KEY_A) || has_key(fd, KEY_ESC)) {
        fds_.push_back(fd);
        std::fprintf(stderr, "keyboard: %s\n", path.c_str());
        // Test runs over SSH: keep the keys from the launcher behind this app. Off by default,
        // because the OS must still see the long Esc press that goes home.
        if (std::getenv("ZEROFIN_GRAB_KEYBOARD") && ioctl(fd, EVIOCGRAB, 1) < 0) {
          std::fprintf(stderr, "cannot grab %s\n", path.c_str());
        }
      } else {
        close(fd);
      }
    }
    closedir(dir);
  }

  void handle(unsigned code, int value, std::vector<ui::KeyEvent>& out) {
    if (log_keys_) std::fprintf(stderr, "key code=%u value=%d\n", code, value);
    if (ui::is_shift_code(code)) {
      shift_ = value != 0;
      return;
    }
    if (code == ui::evdev::kCapsLock) {
      if (value == 1) caps_ = !caps_;
      return;
    }
    if (value == 0) return;  // key release
    ui::KeyEvent key = ui::map_evdev_key(code, shift_);
    if (key.key == ui::Key::Char && caps_) {
      // Caps Lock flips the case of letters only.
      const ui::KeyEvent plain = ui::map_evdev_key(code, false);
      const ui::KeyEvent upper = ui::map_evdev_key(code, true);
      if (plain.ch != upper.ch && std::isalpha(static_cast<unsigned char>(plain.ch))) {
        key = shift_ ? plain : upper;
      }
    }
    if (key.key == ui::Key::None) return;
    key.repeat = value == 2;
    out.push_back(key);
  }

  lv_display_t* disp_ = nullptr;
  std::string fb_path_;
  std::vector<int> fds_;
  bool shift_ = false;
  bool caps_ = false;
  bool log_keys_ = false;
};

}  // namespace

std::unique_ptr<Platform> create_platform() { return std::make_unique<FbPlatform>(); }

}  // namespace zerofin::platform
