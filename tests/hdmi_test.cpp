#include <doctest/doctest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "player/hdmi.h"

using namespace zerofin::player;
namespace fs = std::filesystem;

namespace {

// A fresh folder that removes itself.
struct TempDir {
  fs::path path;
  TempDir() {
    static int n = 0;
    path = fs::temp_directory_path() / ("zerofin-hdmi-test-" + std::to_string(::getpid()) + "-" + std::to_string(n++));
    fs::create_directories(path);
  }
  ~TempDir() { fs::remove_all(path); }
  void write(const std::string& relative, const std::string& text) const {
    fs::create_directories((path / relative).parent_path());
    std::ofstream(path / relative) << text;
  }
};

}  // namespace

TEST_CASE("hdmi: a connected HDMI connector is found") {
  TempDir d;
  d.write("card1-SPI-1/status", "connected\n");
  d.write("card0-HDMI-A-1/status", "connected\n");
  const auto c = find_connected_hdmi(d.path);
  REQUIRE(c.has_value());
  CHECK(c->device == "/dev/dri/card0");
  CHECK(c->connector == "HDMI-A-1");
  CHECK(hdmi_connected(d.path));
}

TEST_CASE("hdmi: a disconnected connector is not found") {
  TempDir d;
  d.write("card0-HDMI-A-1/status", "disconnected\n");
  CHECK_FALSE(find_connected_hdmi(d.path).has_value());
  CHECK_FALSE(hdmi_connected(d.path));
}

TEST_CASE("hdmi: only the LCD connector, so no HDMI") {
  TempDir d;
  d.write("card1-SPI-1/status", "connected\n");
  CHECK_FALSE(hdmi_connected(d.path));
}

TEST_CASE("hdmi: a missing folder is not an error") {
  CHECK_FALSE(hdmi_connected("/this/path/does/not/exist"));
}

TEST_CASE("hdmi: a connector with no status file is skipped") {
  TempDir d;
  fs::create_directories(d.path / "card0-HDMI-A-1");
  d.write("card0-HDMI-A-2/status", "connected\n");
  const auto c = find_connected_hdmi(d.path);
  REQUIRE(c.has_value());
  CHECK(c->connector == "HDMI-A-2");
}

TEST_CASE("hdmi: the card number can be above 9") {
  TempDir d;
  d.write("card12-HDMI-A-1/status", "connected");  // no new line
  const auto c = find_connected_hdmi(d.path);
  REQUIRE(c.has_value());
  CHECK(c->device == "/dev/dri/card12");
}

TEST_CASE("output: Auto follows the cable, LcdOnly never uses HDMI") {
  CHECK(choose_output(OutputSetting::Auto, true) == Output::Hdmi);
  CHECK(choose_output(OutputSetting::Auto, false) == Output::Lcd);
  CHECK(choose_output(OutputSetting::LcdOnly, true) == Output::Lcd);
  CHECK(choose_output(OutputSetting::LcdOnly, false) == Output::Lcd);
}

TEST_CASE("output: the setting text") {
  CHECK(output_setting_from_text("auto") == OutputSetting::Auto);
  CHECK(output_setting_from_text("lcd") == OutputSetting::LcdOnly);
  CHECK(output_setting_from_text("LCD") == OutputSetting::LcdOnly);
  CHECK(output_setting_from_text("") == OutputSetting::Auto);
  CHECK(output_setting_from_text("nonsense") == OutputSetting::Auto);
  CHECK(std::string(output_setting_text(OutputSetting::Auto)) == "auto");
  CHECK(std::string(output_setting_text(OutputSetting::LcdOnly)) == "lcd");
}

TEST_CASE("eld: the LG sample lists one audio format") {
  const std::string eld =
      "monitor_name\t\tLG Ultra HD\nconnection_type\t\tHDMI\nspeakers\t\t[0xffff] FL/FR\nsad_count\t\t1\n"
      "sad0_coding_type\t[0x1] LPCM\nsad0_channels\t\t2\n";
  CHECK(eld_has_audio(eld));
}

TEST_CASE("eld: sad_count 0 means no audio") {
  CHECK_FALSE(eld_has_audio("monitor_name\t\tOld Monitor\nsad_count\t\t0\n"));
}

TEST_CASE("eld: an empty ELD (no monitor data yet) means no audio") {
  CHECK_FALSE(eld_has_audio(""));
}

TEST_CASE("eld: the first matching file decides, no ELD file means unknown (assume audio)") {
  TempDir d;
  CHECK(monitor_has_audio(d.path));  // nothing there: unknown
  d.write("card0/eld#0.0", "monitor_name\t\tA\nsad_count\t\t0\n");
  CHECK_FALSE(monitor_has_audio(d.path));
  d.write("card1/eld#0.0", "monitor_name\t\tB\nsad_count\t\t2\n");
  CHECK(monitor_has_audio(d.path));  // any monitor with audio is enough
}
