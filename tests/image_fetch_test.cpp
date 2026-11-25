#include <doctest/doctest.h>

#include "jellyfin/image_fetch.h"

using namespace zerofin::jf;

namespace {

class OneReply : public HttpClient {
 public:
  explicit OneReply(HttpResponse r) : reply_(std::move(r)) {}
  HttpResponse send(const HttpRequest&) override { return reply_; }

 private:
  HttpResponse reply_;
};

// SOI, the JFIF APP0 segment (length 16), a frame header (length 11), the start of scan, a body and EOI.
// LVGL's TJPGD decoder opens a JPEG from memory only when it starts with exactly these JFIF bytes.
const std::string kJfifStart = std::string("\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00", 20);

std::string jpeg_with_frame(unsigned char frame_marker, const std::string& start = kJfifStart, bool end_marker = true) {
  std::string s = start;
  // Frame header: length 11, 8 bits, height 96, width 64, one component.
  s += std::string("\xFF", 1) + static_cast<char>(frame_marker) + std::string("\x00\x0B\x08\x00\x60\x00\x40\x01\x01\x11\x00", 11);
  s += std::string("\xFF\xDA\x00\x02", 4) + "data";
  if (end_marker) s += std::string("\xFF\xD9", 2);
  return s;
}
const std::string kBaseline = jpeg_with_frame(0xC0);
const std::string kProgressive = jpeg_with_frame(0xC2);

}  // namespace

TEST_CASE("a baseline JPEG is returned") {
  OneReply http({200, kBaseline, {}});
  auto r = fetch_image(http, HttpRequest{});
  REQUIRE(r.ok());
  CHECK(r.value() == kBaseline);
}

TEST_CASE("a 200 with an HTML body is a Parse error") {
  OneReply http({200, "<html>login</html>", {}});
  auto r = fetch_image(http, HttpRequest{});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Parse);
}

TEST_CASE("a progressive JPEG is a Parse error") {
  OneReply http({200, kProgressive, {}});
  auto r = fetch_image(http, HttpRequest{});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Parse);
}

TEST_CASE("404 is NotFound and 401 is Unauthorized") {
  OneReply missing({404, {}, {}});
  CHECK(fetch_image(missing, HttpRequest{}).error().kind == ErrorKind::NotFound);
  OneReply denied({401, {}, {}});
  CHECK(fetch_image(denied, HttpRequest{}).error().kind == ErrorKind::Unauthorized);
  OneReply broken({500, {}, {}});
  CHECK(fetch_image(broken, HttpRequest{}).error().kind == ErrorKind::Http);
}

TEST_CASE("a transport failure is Network") {
  OneReply http({0, {}, "could not connect"});
  CHECK(fetch_image(http, HttpRequest{}).error().kind == ErrorKind::Network);
}

TEST_CASE("is_baseline_jpeg checks the header and the frame type") {
  CHECK(is_baseline_jpeg(kBaseline));
  CHECK_FALSE(is_baseline_jpeg(kProgressive));
  CHECK_FALSE(is_baseline_jpeg(""));
  CHECK_FALSE(is_baseline_jpeg("GIF89a"));
}

TEST_CASE("jpeg_size reads the frame size") {
  const auto size = jpeg_size(kBaseline);
  REQUIRE(size.has_value());
  CHECK(size->first == 64);
  CHECK(size->second == 96);
  CHECK_FALSE(jpeg_size("").has_value());
  CHECK_FALSE(jpeg_size(kProgressive).has_value());
}

TEST_CASE("a truncated JPEG is rejected") {
  const std::string cut = jpeg_with_frame(0xC0, kJfifStart, false) + "curl: (28) Operation timed out";
  CHECK_FALSE(is_baseline_jpeg(cut));
  CHECK_FALSE(jpeg_size(cut).has_value());
  OneReply http({200, cut, {}});
  CHECK(fetch_image(http, HttpRequest{}).error().kind == ErrorKind::Parse);
}

TEST_CASE("a JPEG that TJPGD cannot open from memory is rejected") {
  // An Exif (APP1) segment first, and a frame type TJPGD does not read (SOF1).
  const std::string exif_start = std::string("\xFF\xD8\xFF\xE1\x00\x04", 6) + "ab";
  CHECK_FALSE(is_baseline_jpeg(jpeg_with_frame(0xC0, exif_start)));
  CHECK_FALSE(is_baseline_jpeg(jpeg_with_frame(0xC1)));
}
