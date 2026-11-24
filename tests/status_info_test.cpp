#include <doctest/doctest.h>

#include "ui_core/status_info.h"

using namespace zerofin::ui;

TEST_CASE("parse_battery reads percent and charging state") {
  auto a = parse_battery("67\n", "Discharging\n");
  REQUIRE(a.has_value());
  CHECK(a->percent == 67);
  CHECK_FALSE(a->charging);
  auto b = parse_battery("100", "Charging");
  REQUIRE(b.has_value());
  CHECK(b->percent == 100);
  CHECK(b->charging);
}

TEST_CASE("parse_battery rejects bad text") {
  CHECK_FALSE(parse_battery("101", "Charging").has_value());
  CHECK_FALSE(parse_battery("abc", "Charging").has_value());
  CHECK_FALSE(parse_battery("", "").has_value());
  CHECK_FALSE(parse_battery("-1", "Charging").has_value());
}

TEST_CASE("parse_wireless_dbm reads the level of the first interface") {
  const std::string text =
      "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
      " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n"
      " wlan0: 0000   70.  -23.  -256        0      0      0      3      0        0\n";
  auto dbm = parse_wireless_dbm(text);
  REQUIRE(dbm.has_value());
  CHECK(*dbm == -23);
}

TEST_CASE("parse_wireless_dbm gives nothing without a usable line") {
  CHECK_FALSE(parse_wireless_dbm("").has_value());
  CHECK_FALSE(parse_wireless_dbm("Inter-| sta-|   Quality\n face | tus | link level noise\n").has_value());
  CHECK_FALSE(parse_wireless_dbm(" wlan0: zzzz\n").has_value());
}

TEST_CASE("signal_bars") {
  CHECK(signal_bars(-50) == 4);
  CHECK(signal_bars(-55) == 4);
  CHECK(signal_bars(-56) == 3);
  CHECK(signal_bars(-65) == 3);
  CHECK(signal_bars(-75) == 2);
  CHECK(signal_bars(-85) == 1);
  CHECK(signal_bars(-86) == 0);
}

TEST_CASE("format_clock pads with zeros") {
  CHECK(format_clock(9, 5) == "09:05");
  CHECK(format_clock(20, 29) == "20:29");
}
