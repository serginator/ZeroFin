#pragma once

#include <optional>
#include <string>
#include <utility>
#include <string_view>

#include "jellyfin/http.h"
#include "jellyfin/result.h"

namespace zerofin::jf {

// True when the bytes start like a JPEG (FF D8 FF) and the frame is baseline. The LVGL decoder (TJPGD)
// cannot read a progressive JPEG (SOF2, FF C2).
bool is_baseline_jpeg(std::string_view bytes);

// Width and height from the frame header, or nullopt when the bytes are not a baseline JPEG.
std::optional<std::pair<int, int>> jpeg_size(std::string_view bytes);

// Sends a poster request. Gives the JPEG bytes only for a status 200 body that is a baseline JPEG.
// It does not use a Client, so it can run on the image worker while the main worker uses the Client.
Result<std::string> fetch_image(HttpClient& http, const HttpRequest& request);

}  // namespace zerofin::jf
