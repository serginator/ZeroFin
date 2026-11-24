#include <doctest/doctest.h>

#include "jellyfin/discovery.h"

using namespace zerofin::jf;

TEST_CASE("parse_discovery_reply reads a server reply") {
  auto r = parse_discovery_reply(
      R"({"Address":"http://192.168.1.157:8096","Id":"f6c179d3","Name":"jellyPi","EndpointAddress":null})");
  REQUIRE(r.ok());
  CHECK(r.value().address == "http://192.168.1.157:8096");
  CHECK(r.value().id == "f6c179d3");
  CHECK(r.value().name == "jellyPi");
}

TEST_CASE("parse_discovery_reply rejects incomplete or bad replies") {
  CHECK_FALSE(parse_discovery_reply(R"({"Name":"x"})").ok());
  CHECK_FALSE(parse_discovery_reply("garbage").ok());
  CHECK_FALSE(parse_discovery_reply("[]").ok());
}
