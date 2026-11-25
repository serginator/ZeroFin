#include "ui/assets.h"

#include <cstdlib>
#include <filesystem>
#include <vector>

#include "ui/theme.h"

namespace zerofin::app {

std::string find_asset(const std::string& relative_path) {
  std::vector<std::filesystem::path> roots;
  if (const char* env = std::getenv("ZEROFIN_ASSETS"); env && *env) roots.emplace_back(env);
  roots.emplace_back(ZEROFIN_ASSETS_SOURCE_DIR);
  roots.emplace_back("/usr/share/zerofin");
  for (const auto& root : roots) {
    const auto candidate = root / relative_path;
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec)) return candidate.string();
  }
  return {};
}

}  // namespace zerofin::app
