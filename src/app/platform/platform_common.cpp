// Code shared by the SDL simulator and the framebuffer device platform.
#include "platform/platform_common.h"

#include <cstdio>
#include <ctime>

#include "platform/platform.h"

namespace zerofin::platform {

uint32_t tick_ms() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

bool write_ppm(const std::string& path, int w, int h, const uint8_t* rgb888) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  std::fwrite(rgb888, 1, static_cast<size_t>(w) * static_cast<size_t>(h) * 3, f);
  std::fclose(f);
  return true;
}

}  // namespace zerofin::platform
