#include <doctest/doctest.h>

#include "jellyfin/auth.h"

using namespace zerofin::jf;

TEST_CASE("authorization header without a token") {
  ClientInfo info;
  info.device_id = "abc";
  CHECK(authorization_header_value(info, "") ==
        "MediaBrowser Client=\"ZeroFin\", Device=\"Cardputer Zero\", DeviceId=\"abc\", Version=\"0.1.0\"");
}

TEST_CASE("authorization header with a token") {
  ClientInfo info;
  info.device_id = "abc";
  const std::string value = authorization_header_value(info, "tok");
  const std::string suffix = ", Token=\"tok\"";
  REQUIRE(value.size() > suffix.size());
  CHECK(value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0);
}

TEST_CASE("authorization header removes double quotes from values") {
  ClientInfo info;
  info.device = "Say \"hi\"";
  info.device_id = "abc";
  CHECK(authorization_header_value(info, "").find("Device=\"Say hi\"") != std::string::npos);
}

TEST_CASE("random_hex_id makes 32 hex characters and differs between calls") {
  const std::string a = random_hex_id();
  const std::string b = random_hex_id();
  CHECK(a.size() == 32);
  CHECK(a.find_first_not_of("0123456789abcdef") == std::string::npos);
  CHECK(a != b);
}
