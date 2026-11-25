#include "jellyfin/image_fetch.h"

namespace zerofin::jf {

namespace {

// What LVGL's TJPGD decoder needs to open a JPEG from memory: this exact start (SOI, then a JFIF APP0 segment).
constexpr unsigned char kJfifStart[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46};

// Walks the marker segments up to the start of scan (SOS, FF DA). Gives the size from the first SOF.
// Gives nullopt when TJPGD could not show the picture: no JFIF start, a progressive or extended frame,
// no frame, or no end marker (a download that stopped early).
std::optional<std::pair<int, int>> scan(std::string_view b) {
  auto byte = [&](size_t i) { return static_cast<unsigned char>(b[i]); };
  if (b.size() < sizeof(kJfifStart) + 2) return std::nullopt;
  for (size_t i = 0; i < sizeof(kJfifStart); ++i) {
    if (byte(i) != kJfifStart[i]) return std::nullopt;
  }
  if (byte(b.size() - 2) != 0xFF || byte(b.size() - 1) != 0xD9) return std::nullopt;
  std::optional<std::pair<int, int>> size;
  size_t i = 2;
  while (i + 3 < b.size()) {
    if (byte(i) != 0xFF) return std::nullopt;
    const unsigned char marker = byte(i + 1);
    if (marker == 0xFF) { ++i; continue; }  // fill bytes
    if (marker == 0xC1 || marker == 0xC2) return std::nullopt;  // TJPGD reads baseline (SOF0) only
    if (marker == 0xDA) return size;
    if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) { i += 2; continue; }
    const size_t len = (static_cast<size_t>(byte(i + 2)) << 8) | byte(i + 3);
    if (len < 2) return std::nullopt;
    if (marker == 0xC0 && len >= 8 && i + 8 < b.size()) {
      const int h = (byte(i + 5) << 8) | byte(i + 6);
      const int w = (byte(i + 7) << 8) | byte(i + 8);
      if (w > 0 && h > 0) size = std::make_pair(w, h);
    }
    i += 2 + len;
  }
  return std::nullopt;
}

}  // namespace

bool is_baseline_jpeg(std::string_view bytes) { return scan(bytes).has_value(); }

std::optional<std::pair<int, int>> jpeg_size(std::string_view bytes) { return scan(bytes); }

Result<std::string> fetch_image(HttpClient& http, const HttpRequest& request) {
  HttpResponse res = http.send(request);
  if (res.status == 0) return Error{ErrorKind::Network, res.error.empty() ? "no response" : res.error, 0};
  if (res.status == 401) return Error{ErrorKind::Unauthorized, "the server rejected the login", 401};
  if (res.status == 404) return Error{ErrorKind::NotFound, "no image", 404};
  if (res.status != 200) return Error{ErrorKind::Http, "HTTP " + std::to_string(res.status), res.status};
  if (!is_baseline_jpeg(res.body)) return Error{ErrorKind::Parse, "not a baseline JPEG", 200};
  return std::move(res.body);
}

}  // namespace zerofin::jf
