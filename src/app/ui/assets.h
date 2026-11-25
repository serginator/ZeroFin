#pragma once

#include <string>

namespace zerofin::app {

// Finds a file of the app assets, for example "fonts/DejaVuSansCondensed.ttf" or "images/banner-splash.jpg".
// Looks in $ZEROFIN_ASSETS, then in the source tree, then in /usr/share/zerofin. Returns "" when not found.
std::string find_asset(const std::string& relative_path);

}  // namespace zerofin::app
